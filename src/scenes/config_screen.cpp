#include "scenes/config_screen.h"
#include "core/zip_utils.h"
#include "core/assets.h"
#include "core/gamepad.h"
#include "core/log.h"
#include "core/embedded_loader.h"
#include "gfx/SFont.h"
#include "ui/button.h"

extern "C" {
    #include "embedded_resources.h"
}

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <cctype>

#define GAME_WIDTH  320
#define GAME_HEIGHT 200


std::vector<ZipPackStatus> ScanDirectory(const fs::path& dir) {
    std::vector<ZipPackStatus> results;
    for (const ZipPack& pack : GetZipPacks()) {
        ZipPackStatus s;
        s.pack = pack;
        fs::path zipPath = dir / pack.filename;
        s.zipFound = fs::exists(zipPath);
        s.zipValid = false;
        if (s.zipFound) {
            s.zipValid = true;
            for (const std::string& entry : pack.adfContents) {
                if (!ZipContainsFile(zipPath, entry)) {
                    s.zipValid = false;
                    break;
                }
            }
            // A pack with no required contents is valid when found.
        }
        if (!s.zipValid && !pack.adfContents.empty()) {
            s.zipValid = std::all_of(pack.adfContents.begin(), pack.adfContents.end(),
                [&](const std::string& adf) { return fs::exists(dir / adf); });
        }
        results.push_back(s);
    }
    return results;
}

// So parent_path() steps up once per press.
static fs::path Normalize(const fs::path& p) {
    std::error_code ec;
    fs::path c = fs::weakly_canonical(p, ec);
    return ec ? p : c;
}

// Subdirectories of dir, name-sorted, dotfiles and unreadable entries skipped.
static std::vector<fs::path> ListDirs(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec)) {
        if (!it->is_directory(ec) || ec) {
            ec.clear();
            continue;
        }
        const std::string name = it->path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        out.push_back(it->path());
    }
    auto lower = [](std::string v) {
        std::transform(v.begin(), v.end(), v.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return v;
    };
    std::sort(out.begin(), out.end(), [&](const fs::path& a, const fs::path& b) {
        return lower(a.filename().string()) < lower(b.filename().string());
    });
    return out;
}

// Cheap "worth looking in here" test for the rows: one mandatory zip or its
// first loose ADF exists by name. No zip is opened - that would mean unpacking every sibling folder just
static bool HasMandatoryZip(const fs::path& dir) {
    for (const ZipPack& pack : GetZipPacks()) {
        if (!pack.mandatory) continue;
        std::error_code ec;
        if (fs::exists(dir / pack.filename, ec) && !ec) return true;
        if (!pack.adfContents.empty() && fs::exists(dir / pack.adfContents[0], ec) && !ec) return true;
    }
    return false;
}

// Trim from the left, marking the cut, so a long path still shows its tail.
static std::string FitLeft(SFont_Font* f, std::string s, int maxW) {
    if (SFont_TextWidth(f, s.c_str()) <= maxW) return s;
    while (!s.empty() && SFont_TextWidth(f, ("..." + s).c_str()) > maxW) s.erase(0, 1);
    return "..." + s;
}

static std::string FitRight(SFont_Font* f, std::string s, int maxW) {
    while (!s.empty() && SFont_TextWidth(f, s.c_str()) > maxW) s.pop_back();
    return s;
}

static void DrawFrame(SDL_Surface* surf) {
    SDL_FillRect(surf, nullptr, SDL_MapRGB(surf->format, 0, 208, 0));

    const int borderThickness = 1;
    Uint32 white = SDL_MapRGB(surf->format, 255, 255, 255);
    SDL_Rect top    = {1, 1, surf->w - 2, borderThickness};
    SDL_Rect bottom = {1, surf->h - borderThickness, surf->w - 2, borderThickness};
    SDL_Rect left   = {1, 1, borderThickness, surf->h - 2};
    SDL_Rect right  = {surf->w - borderThickness, 1, borderThickness, surf->h - 2};
    SDL_FillRect(surf, &top, white);
    SDL_FillRect(surf, &bottom, white);
    SDL_FillRect(surf, &left, white);
    SDL_FillRect(surf, &right, white);
}

// The folder browser: a scrolling list of subdirectories, with the accept row
// pinned at the top so one button does everything.
static void DrawBrowser(SDL_Surface* surf,
                        SFont_Font* headingFont, SFont_Font* bodyFont,
                        const fs::path& dir,
                        const std::vector<std::string>& labels,
                        const std::string& status, bool anyMarked,
                        int sel, int scroll, int top, int rowH, int rows) {
    DrawFrame(surf);

    SFont_WriteCenter(surf, headingFont, 4, "Select folder");
    SFont_Write(surf, bodyFont, 5, top - rowH * 2 - 2,
                FitLeft(bodyFont, dir.string(), GAME_WIDTH - 10).c_str());
    SFont_Write(surf, bodyFont, 5, top - rowH - 2,
                FitRight(bodyFont, status, GAME_WIDTH - 10).c_str());

    const Uint32 yellow = SDL_MapRGB(surf->format, 255, 255, 0);
    const int last = std::min((int)labels.size(), scroll + rows);
    for (int i = scroll; i < last; ++i) {
        const int y = top + (i - scroll) * rowH;
        if (i == sel) {
            SDL_Rect box = {3, y - 1, GAME_WIDTH - 6, rowH};
            SDL_Rect in  = {4, y,     GAME_WIDTH - 8, rowH - 2};
            SDL_FillRect(surf, &box, yellow);
            SDL_FillRect(surf, &in, SDL_MapRGB(surf->format, 0, 208, 0));
        }
        SFont_Write(surf, bodyFont, 7, y,
                    FitRight(bodyFont, labels[i], GAME_WIDTH - 14).c_str());
    }

    if (anyMarked) {
        SFont_WriteCenter(surf, bodyFont, GAME_HEIGHT - rowH * 2 - 4,
                          "* folder holds game files");
    }
    SFont_WriteCenter(surf, bodyFont, GAME_HEIGHT - rowH - 3,
                      "Enter/A open   Esc/B cancel");
}

static void DrawScreen(SDL_Surface* surf,
                       SFont_Font* headingFont, SFont_Font* bodyFont,
                       const std::vector<ZipPackStatus>& status,
                       const fs::path& currentDir,
                       bool canStart) {
    DrawFrame(surf);

    int headH = SFont_TextHeight(headingFont) + 2;
    int lineH  = SFont_TextHeight(bodyFont)   + 2;
    int y = 4;

    SFont_WriteCenter(surf, headingFont, y, "RodLegacy Setup");
    y += headH;
    SFont_WriteCenter(surf, bodyFont, y, "Welcome to RodLegacy. You need to");
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "select where Street Rod 1 or 2");
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "game files are located.");
    y += lineH * 2;

    if (!currentDir.empty()) {
        // A trailing separator (e.g. from the folder picker) makes filename()
        // return empty; fall back to the parent path's filename in that case.
        fs::path dirPath = currentDir.filename().empty() ? currentDir.parent_path() : currentDir;
        std::string dirStr = "Dir: " + dirPath.filename().string();
        SFont_Write(surf, bodyFont, 5, y, dirStr.c_str());
        y += lineH;
    }
    y += lineH / 2;

    for (const ZipPackStatus& s : status) {
        const char* tag = (s.zipValid ? "[FOUND]  " : "[MISSING]");
        std::string line = std::string(tag) + " " + s.pack.filename;
        if (s.pack.mandatory) line += " *";
        SFont_Write(surf, bodyFont, 5, y, line.c_str());
        y += lineH;
    }

    y += lineH / 2;

    if (!canStart) {
        SFont_WriteCenter(surf, bodyFont, y, "* one of these is required");
    }
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "or drop a folder or zip on this window");
}

fs::path RunConfigScreen(SDL_Renderer* renderer) {
    SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    SDL_Surface* beeSurf = LoadSurfaceFromMemory(embedded_bee_png, embedded_bee_png_len);
    if (!beeSurf) {
        LOG_WARN("ConfigScreen: failed to load embedded bee font");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }
    SFont_Font* headingFont = SFont_InitFont(beeSurf);
    if (!headingFont) {
        SDL_FreeSurface(beeSurf);
        LOG_WARN("ConfigScreen: failed to init bee SFont");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }

    SDL_Surface* g1Surf = LoadSurfaceFromMemory(embedded_g1_7x8_sfont_png, embedded_g1_7x8_sfont_png_len);
    if (!g1Surf) {
        SFont_FreeFont(headingFont);
        LOG_WARN("ConfigScreen: failed to load embedded g1-7x8 font");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }
    SFont_Font* bodyFont = SFont_InitFont(g1Surf);
    if (!bodyFont) {
        SDL_FreeSurface(g1Surf);
        SFont_FreeFont(headingFont);
        LOG_WARN("ConfigScreen: failed to init g1-7x8 SFont");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        SFont_FreeFont(bodyFont);
        SFont_FreeFont(headingFont);
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }

    // The list FindGameDirectory() would try.
    std::vector<fs::path> searchDirs;
    for (const auto& entry : AssetManager::GameSearchPaths()) {
        searchDirs.push_back(entry.path);
    }

    fs::path currentDir;
    std::vector<ZipPackStatus> packStatus;
    bool canStart = false;

    for (const fs::path& d : searchDirs) {
        auto s = ScanDirectory(d);
        if (!AnyMandatoryValid(s)) continue;
        currentDir = d;
        packStatus = s;
        canStart = true;
        break;
    }
    if (currentDir.empty()) {
        currentDir = fs::current_path();
        packStatus = ScanDirectory(currentDir);
    }

    fs::path result;
    bool running = true;

    // Folder browser state. Row 0 accepts the folder being shown; the rest are
    // the parent and the subdirectories, so one button (A / Enter) is the whole
    // interaction and it drives from a pad, the keyboard or the mouse alike.
    bool browsing = false;
    fs::path browseDir;
    std::vector<std::string> rowLabels;
    std::vector<fs::path> rowTargets;   // empty = "use browseDir"
    std::string browseStatus;
    bool browseAnyMarked = false;
    int browseSel = 0, browseScroll = 0, browseStickDir = 0;

    const int rowH = SFont_TextHeight(bodyFont) + 2;
    const int browseTop = 44;
    const int browseRows = (GAME_HEIGHT - 8 - rowH * 3 - browseTop) / rowH;

    auto refreshBrowse = [&]() {
        rowLabels.clear();
        rowTargets.clear();
        browseAnyMarked = false;

        // Full check for the folder being shown, since it is the one the user can accept.
        browseStatus.clear();
        for (const ZipPackStatus& ps : ScanDirectory(browseDir)) {
            if (!browseStatus.empty()) browseStatus += "  ";
            browseStatus += (ps.zipValid ? "[+] " : "[-] ") + ps.pack.filename;
        }

        rowLabels.push_back("[ Use this folder ]");
        rowTargets.push_back({});

        const fs::path parent = browseDir.parent_path();
        if (!parent.empty() && parent != browseDir) {
            rowLabels.push_back("..");
            rowTargets.push_back(parent);
        } else {
#ifdef _WIN32
            // At a drive root there is no parent, so offer the other drives.
            for (char d = 'A'; d <= 'Z'; ++d) {
                const fs::path drive = std::string(1, d) + ":\\";
                std::error_code ec;
                if (fs::is_directory(drive, ec)) {
                    rowLabels.push_back(std::string(1, d) + ":");
                    rowTargets.push_back(drive);
                }
            }
#endif
        }

        for (const fs::path& d : ListDirs(browseDir)) {
            const bool marked = HasMandatoryZip(d);
            browseAnyMarked = browseAnyMarked || marked;
            rowLabels.push_back(d.filename().string() + (marked ? "  *" : ""));
            rowTargets.push_back(d);
        }
        browseSel = 0;
        browseScroll = 0;
    };

    auto useFolder = [&](const fs::path& dir) {
        packStatus = ScanDirectory(dir);
        currentDir = dir;
        canStart = AnyMandatoryValid(packStatus);
        browsing = false;
    };

    auto browseForFolder = [&]() {
        browseDir = Normalize(currentDir.empty() ? fs::current_path() : currentDir);
        refreshBrowse();
        browsing = true;
    };

    auto browseMove = [&](int delta) {
        const int n = (int)rowLabels.size();
        if (n == 0) return;
        browseSel = std::max(0, std::min(browseSel + delta, n - 1));
        if (browseSel < browseScroll) browseScroll = browseSel;
        if (browseSel >= browseScroll + browseRows) browseScroll = browseSel - browseRows + 1;
    };

    auto browseActivate = [&]() {
        if (browseSel < 0 || browseSel >= (int)rowTargets.size()) return;
        if (rowTargets[browseSel].empty()) {
            useFolder(browseDir);
            return;
        }
        browseDir = Normalize(rowTargets[browseSel]);
        refreshBrowse();
    };

    auto confirmStart = [&]() {
        if (canStart) {
            result = currentDir;
            running = false;
        }
    };

    const int btnW = 90, btnH = 16, btnY = GAME_HEIGHT - 22, btnGap = 10;
    const int btnX = (GAME_WIDTH - (btnW * 3 + btnGap * 2)) / 2;

    ButtonConfig browseCfg{};
    browseCfg.bounds = {btnX, btnY, btnW, btnH};
    browseCfg.text = "Browse";
    browseCfg.font = bodyFont;
    browseCfg.onClick = browseForFolder;
    browseCfg.fillColor = {0, 0, 0, 255};
    browseCfg.borderColor = {255, 255, 255, 255};
    browseCfg.textColor = {255, 255, 255, 255};
    Button browseButton(browseCfg);

    // Button has no live enable toggle, so it is rebuilt when canStart changes.
    const SDL_Color startEnabledColor  = {255, 255, 255, 255};
    const SDL_Color startDisabledColor = {110, 110, 110, 255};
    auto makeStartButton = [&]() {
        ButtonConfig cfg{};
        cfg.bounds = {btnX + btnW + btnGap, btnY, btnW, btnH};
        cfg.text = "Start game";
        cfg.font = bodyFont;
        cfg.onClick = canStart ? confirmStart : std::function<void()>{};
        cfg.fillColor = {0, 0, 0, 255};
        cfg.borderColor = canStart ? startEnabledColor : startDisabledColor;
        cfg.textColor = canStart ? startEnabledColor : startDisabledColor;
        return std::make_unique<Button>(cfg);
    };
    std::unique_ptr<Button> startButton = makeStartButton();
    bool startButtonWasEnabled = canStart;

    ButtonConfig quitCfg{};
    quitCfg.bounds = {btnX + (btnW + btnGap) * 2, btnY, btnW, btnH};
    quitCfg.text = "Quit";
    quitCfg.font = bodyFont;
    quitCfg.onClick = [&]() { running = false; };
    quitCfg.fillColor = {0, 0, 0, 255};
    quitCfg.borderColor = {255, 255, 255, 255};
    quitCfg.textColor = {255, 255, 255, 255};
    Button quitButton(quitCfg);

    // Start is skipped while disabled.
    int focus = canStart ? 1 : 0;
    auto focusEnabled = [&](int i) { return i != 1 || canStart; };
    auto moveFocus = [&](int delta) {
        for (int i = 0; i < 3; ++i) {
            focus = (focus + delta + 3) % 3;
            if (focusEnabled(focus)) return;
        }
    };
    auto activateFocus = [&]() {
        if (focus == 0) browseButton.SimulateClick();
        else if (focus == 1) startButton->SimulateClick();
        else quitButton.SimulateClick();
    };
    int stickDir = 0;  // latched, so one stick push moves focus one step

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }
            gamepad.HandleEvent(e);

            // A dropped file means its folder.
            if (e.type == SDL_DROPFILE) {
                fs::path dropped = e.drop.file ? fs::path(e.drop.file) : fs::path();
                SDL_free(e.drop.file);
                std::error_code ec;
                if (!dropped.empty() && (!fs::is_directory(dropped, ec) || ec)) {
                    dropped = dropped.parent_path();
                }
                if (!dropped.empty()) useFolder(Normalize(dropped));
                continue;
            }

            if (browsing) {
                if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.scancode) {
                        case SDL_SCANCODE_UP:       browseMove(-1); break;
                        case SDL_SCANCODE_DOWN:     browseMove(1); break;
                        case SDL_SCANCODE_PAGEUP:   browseMove(-browseRows); break;
                        case SDL_SCANCODE_PAGEDOWN: browseMove(browseRows); break;
                        case SDL_SCANCODE_HOME:     browseMove(-(int)rowLabels.size()); break;
                        case SDL_SCANCODE_END:      browseMove((int)rowLabels.size()); break;
                        case SDL_SCANCODE_RETURN:
                        case SDL_SCANCODE_KP_ENTER: browseActivate(); break;
                        case SDL_SCANCODE_ESCAPE:   browsing = false; break;
                        default: break;
                    }
                } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                    switch (e.cbutton.button) {
                        case SDL_CONTROLLER_BUTTON_DPAD_UP:    browseMove(-1); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  browseMove(1); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  browseMove(-browseRows); break;
                        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: browseMove(browseRows); break;
                        case SDL_CONTROLLER_BUTTON_A:
                        case SDL_CONTROLLER_BUTTON_START:      browseActivate(); break;
                        case SDL_CONTROLLER_BUTTON_B:          browsing = false; break;
                        default: break;
                    }
                } else if (e.type == SDL_CONTROLLERAXISMOTION &&
                           e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                    const int dir = e.caxis.value < -Gamepad::DEADZONE ? -1 :
                                    e.caxis.value >  Gamepad::DEADZONE ?  1 : 0;
                    if (dir != 0 && dir != browseStickDir) browseMove(dir);
                    browseStickDir = dir;
                } else if (e.type == SDL_MOUSEWHEEL) {
                    browseMove(-e.wheel.y);
                } else if (e.type == SDL_MOUSEBUTTONDOWN &&
                           e.button.button == SDL_BUTTON_LEFT) {
                    const int row = browseScroll + (e.button.y - browseTop) / rowH;
                    if (e.button.y >= browseTop && row >= 0 &&
                        row < (int)rowLabels.size() && row < browseScroll + browseRows) {
                        browseSel = row;
                        browseActivate();
                    }
                }
                continue;
            }

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_Q:
                    case SDL_SCANCODE_ESCAPE:
                        running = false;
                        break;
                    case SDL_SCANCODE_F:
                        browseForFolder();
                        break;
                    case SDL_SCANCODE_LEFT:
                        moveFocus(-1);
                        break;
                    case SDL_SCANCODE_RIGHT:
                    case SDL_SCANCODE_TAB:
                        moveFocus(1);
                        break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_KP_ENTER:
                        activateFocus();
                        break;
                    default:
                        break;
                }
            }
            if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                        moveFocus(-1);
                        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                        moveFocus(1);
                        break;
                    case SDL_CONTROLLER_BUTTON_A:
                    case SDL_CONTROLLER_BUTTON_START:
                        activateFocus();
                        break;
                    case SDL_CONTROLLER_BUTTON_B:
                        running = false;
                        break;
                    case SDL_CONTROLLER_BUTTON_X:
                    case SDL_CONTROLLER_BUTTON_Y:
                        browseForFolder();
                        break;
                    default:
                        break;
                }
            }
            // Pads that map the stick instead of a d-pad still navigate.
            if (e.type == SDL_CONTROLLERAXISMOTION &&
                e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) {
                const int dir = e.caxis.value < -Gamepad::DEADZONE ? -1 :
                                e.caxis.value >  Gamepad::DEADZONE ?  1 : 0;
                if (dir != 0 && dir != stickDir) moveFocus(dir);
                stickDir = dir;
            }
            browseButton.HandleEvent(&e);
            startButton->HandleEvent(&e);
            quitButton.HandleEvent(&e);
        }

        if (canStart != startButtonWasEnabled) {
            startButton = makeStartButton();
            startButtonWasEnabled = canStart;
            focus = canStart ? 1 : 0;
        }

        if (browsing) {
            DrawBrowser(surf, headingFont, bodyFont, browseDir, rowLabels,
                        browseStatus, browseAnyMarked,
                        browseSel, browseScroll, browseTop, rowH, browseRows);
        } else {
            DrawScreen(surf, headingFont, bodyFont, packStatus, currentDir, canStart);
        }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, tex, nullptr, nullptr);
            SDL_DestroyTexture(tex);
        }
        if (browsing) {
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
        }
        browseButton.Draw();
        startButton->Draw();
        quitButton.Draw();

        const SDL_Rect bounds[3] = {browseButton.GetBounds(),
                                    startButton->GetBounds(),
                                    quitButton.GetBounds()};
        SDL_Rect ring = bounds[focus];
        ring.x -= 2; ring.y -= 2; ring.w += 4; ring.h += 4;
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderDrawRect(renderer, &ring);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);  // ~60 fps, no vsync needed here
    }

    SDL_FreeSurface(surf);
    SFont_FreeFont(bodyFont);
    SFont_FreeFont(headingFont);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    return result;
}

bool RunGamePicker(SDL_Renderer* renderer, SrGame* game) {
    SFont_Font* headingFont = SFont_InitFont(LoadSurfaceFromMemory(embedded_bee_png, embedded_bee_png_len));
    SFont_Font* bodyFont = SFont_InitFont(LoadSurfaceFromMemory(embedded_g1_7x8_sfont_png,
                                                                embedded_g1_7x8_sfont_png_len));
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
    if (!headingFont || !bodyFont || !surf) {
        if (surf) SDL_FreeSurface(surf);
        if (bodyFont) SFont_FreeFont(bodyFont);
        if (headingFont) SFont_FreeFont(headingFont);
        return true;
    }
    SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);

    bool picked = false, running = true;
    const int btnW = 120, btnH = 20, btnX = (GAME_WIDTH - btnW) / 2;
    const SrGame games[2] = {SrGame::SR1, SrGame::SR2};
    const char* labels[2] = {"Street Rod", "Street Rod 2"};
    std::vector<std::unique_ptr<Button>> buttons;
    for (int i = 0; i < 2; ++i) {
        ButtonConfig cfg{};
        cfg.bounds = {btnX, 80 + i * 32, btnW, btnH};
        cfg.text = labels[i];
        cfg.font = bodyFont;
        cfg.onClick = [&, i]() { *game = games[i]; picked = true; running = false; };
        cfg.borderColor = {255, 255, 255, 255};
        cfg.textColor = {255, 255, 255, 255};
        buttons.push_back(std::make_unique<Button>(cfg));
    }
    int focus = *game == SrGame::SR2 ? 1 : 0;
    int stickDir = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; break; }
            gamepad.HandleEvent(e);
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:   running = false; break;
                    case SDL_SCANCODE_UP:
                    case SDL_SCANCODE_DOWN:
                    case SDL_SCANCODE_TAB:      focus ^= 1; break;
                    case SDL_SCANCODE_1:        buttons[0]->SimulateClick(); break;
                    case SDL_SCANCODE_2:        buttons[1]->SimulateClick(); break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_KP_ENTER: buttons[focus]->SimulateClick(); break;
                    default: break;
                }
            } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: focus ^= 1; break;
                    case SDL_CONTROLLER_BUTTON_A:
                    case SDL_CONTROLLER_BUTTON_START:     buttons[focus]->SimulateClick(); break;
                    case SDL_CONTROLLER_BUTTON_B:         running = false; break;
                    default: break;
                }
            } else if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                const int dir = e.caxis.value < -Gamepad::DEADZONE ? -1 :
                                e.caxis.value >  Gamepad::DEADZONE ?  1 : 0;
                if (dir != 0 && dir != stickDir) focus ^= 1;
                stickDir = dir;
            }
            for (auto& b : buttons) b->HandleEvent(&e);
        }

        DrawFrame(surf);
        SFont_WriteCenter(surf, headingFont, 30, "Pick a game");
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, tex, nullptr, nullptr);
            SDL_DestroyTexture(tex);
        }
        for (auto& b : buttons) b->Draw();
        SDL_Rect ring = buttons[focus]->GetBounds();
        ring.x -= 2; ring.y -= 2; ring.w += 4; ring.h += 4;
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_RenderDrawRect(renderer, &ring);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_FreeSurface(surf);
    SFont_FreeFont(bodyFont);
    SFont_FreeFont(headingFont);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    return picked;
}
