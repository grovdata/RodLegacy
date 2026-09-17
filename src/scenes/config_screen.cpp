#include "scenes/config_screen.h"
#include "core/zip_utils.h"
#include "core/gamepad.h"
#include "core/log.h"
#include "core/embedded_loader.h"
#include "gfx/SFont.h"
#include "tinyfiledialogs.h"
#include "ui/button.h"

extern "C" {
    #include "embedded_resources.h"
}

#include <SDL.h>
#include <SDL_image.h>

#define GAME_WIDTH  320
#define GAME_HEIGHT 200

// ---------------------------------------------------------------------------
// Pack definitions
// ---------------------------------------------------------------------------

const std::vector<ZipPack>& GetZipPacks() {
    // Add more later, as they are supported
    static const std::vector<ZipPack> packs = {
        {
            "sr1amiga.zip", true, "Amiga disks (required)",
            {"SR1AMIGA_DISK1.adf", "SR1AMIGA_DISK2.adf"}
        },        
        { "srsepc.zip",       false, "SE/Data Disk (stickers, cars)", {} },        
    };
    return packs;
}

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
            // A pack with no required contents is considered valid when found.
        }
        results.push_back(s);
    }
    return results;
}

// ---------------------------------------------------------------------------
// ConfigScreen rendering helpers
// ---------------------------------------------------------------------------

static void DrawScreen(SDL_Surface* surf,
                       SFont_Font* headingFont, SFont_Font* bodyFont,
                       const std::vector<ZipPackStatus>& status,
                       const fs::path& currentDir,
                       bool canStart) {
    // Dialog green background (matches in-game dialogs, e.g. engine_bay.cpp)
    SDL_FillRect(surf, nullptr, SDL_MapRGB(surf->format, 0, 208, 0));

    // White border around the screen
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

    int headH = SFont_TextHeight(headingFont) + 2;
    int lineH  = SFont_TextHeight(bodyFont)   + 2;
    int y = 4;

    SFont_WriteCenter(surf, headingFont, y, "RodLegacy Setup");
    y += headH;
    SFont_WriteCenter(surf, bodyFont, y, "Welcome to RodLegacy. You need to");
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "select where Street Rod");
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "game files are located.");
    y += lineH * 2;

    // Current directory (just the folder name; full path can be too long to fit)
    if (!currentDir.empty()) {
        // A trailing separator (e.g. from the folder picker) makes filename()
        // return empty; fall back to the parent path's filename in that case.
        fs::path dirPath = currentDir.filename().empty() ? currentDir.parent_path() : currentDir;
        std::string dirStr = "Dir: " + dirPath.filename().string();
        SFont_Write(surf, bodyFont, 5, y, dirStr.c_str());
        y += lineH;
    }
    y += lineH / 2;

    // Pack status list
    for (const ZipPackStatus& s : status) {
        const char* tag = (s.zipValid ? "[FOUND]  " : "[MISSING]");
        std::string line = std::string(tag) + " " + s.pack.filename;
        if (s.pack.mandatory) line += " *";
        SFont_Write(surf, bodyFont, 5, y, line.c_str());
        y += lineH;
    }

    y += lineH / 2;

    if (!canStart) {
        SFont_WriteCenter(surf, bodyFont, y, "* required to play");
    }
}

// ---------------------------------------------------------------------------
// ConfigScreen::Run
// ---------------------------------------------------------------------------

fs::path RunConfigScreen(SDL_Renderer* renderer) {
    SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);

    // Heading font: bee.png
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

    // Body font: g1-7x8
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

    // Working surface (320x200, ARGB)
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, GAME_WIDTH, GAME_HEIGHT, 32,
                                                        SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        SFont_FreeFont(bodyFont);
        SFont_FreeFont(headingFont);
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }

    // Pre-scan paths that FindGameDirectory() would have tried
    std::vector<fs::path> searchDirs = {
        fs::current_path(),
        fs::current_path() / "..",
        fs::current_path() / ".." / "..",
    };
    const char* envPath = getenv("STREETROD_DATA");
    if (envPath) searchDirs.push_back(fs::path(envPath));

    fs::path currentDir;
    std::vector<ZipPackStatus> packStatus;
    bool canStart = false;

    // Find best pre-populated directory (prefer one with mandatory pack)
    for (const fs::path& d : searchDirs) {
        auto s = ScanDirectory(d);
        for (const auto& ps : s) {
            if (ps.pack.mandatory && ps.zipValid) {
                currentDir = d;
                packStatus = s;
                canStart = true;
                break;
            }
        }
        if (canStart) break;
    }
    // Fallback: use current dir with whatever status
    if (currentDir.empty()) {
        currentDir = fs::current_path();
        packStatus = ScanDirectory(currentDir);
    }

    fs::path result;
    bool running = true;

    auto browseForFolder = [&]() {
        const char* picked = tinyfd_selectFolderDialog("Select game data folder", nullptr);
        if (!picked) return;
        fs::path dir = fs::path(picked);
        packStatus = ScanDirectory(dir);
        currentDir = dir;
        canStart = false;
        for (const auto& ps : packStatus) {
            if (ps.pack.mandatory && ps.zipValid) {
                canStart = true;
                break;
            }
        }
    };

    auto confirmStart = [&]() {
        if (canStart) {
            result = currentDir;
            running = false;
        }
    };

    // Bottom-anchored Browse / Start game / Quit buttons.
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

    // Start game is disabled (greyed out, unclickable) while canStart is
    // false. Button has no live enable/disable toggle, so it's rebuilt
    // whenever canStart changes instead.
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

    // Focus for keyboard/gamepad: 0 Browse, 1 Start game, 2 Quit. Start is
    // skipped while it is disabled, so focus never lands on a dead button.
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
            gamepad.HandleEvent(e);
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
            focus = canStart ? 1 : 0;  // point at whatever now matters
        }

        // Render
        DrawScreen(surf, headingFont, bodyFont, packStatus, currentDir, canStart);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, tex, nullptr, nullptr);
            SDL_DestroyTexture(tex);
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
