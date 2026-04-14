#include "config_screen.h"
#include "zip_utils.h"
#include "log.h"
#include "embedded_loader.h"
#include "SFont.h"
#include "tinyfiledialogs.h"

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
    static const std::vector<ZipPack> packs = {
        {
            "sr1amiga.zip", true, "Amiga disks (required)",
            {"SR1AMIGA_DISK1.adf", "SR1AMIGA_DISK2.adf"}
        },
        { "sr1pc.zip",        false, "PC version (music + graphics)", {} },
        { "srsepc.zip",       false, "SE/Data Disk (stickers, cars)", {} },
        { "sr2pc.zip",        false, "Street Rod 2 PC (future)",     {} },
        { "sr2amiga.zip",     false, "Street Rod 2 Amiga (future)",  {} },
        { "sr1commodore.zip", false, "Commodore version (future)",   {} },
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
    // Black background
    SDL_FillRect(surf, nullptr, SDL_MapRGB(surf->format, 0, 0, 0));

    int headH = SFont_TextHeight(headingFont) + 2;
    int lineH  = SFont_TextHeight(bodyFont)   + 2;
    int y = 4;

    // Title in bee font (centered)
    SFont_WriteCenter(surf, headingFont, y, "ROD LEGACY - SETUP");
    y += headH + lineH;

    // Current directory (truncated if too wide)
    if (!currentDir.empty()) {
        std::string dirStr = "Dir: " + currentDir.string();
        while (SFont_TextWidth(bodyFont, dirStr.c_str()) > GAME_WIDTH - 4 && dirStr.size() > 8) {
            dirStr = "..." + dirStr.substr(4);
        }
        SFont_Write(surf, bodyFont, 2, y, dirStr.c_str());
        y += lineH;
    }
    y += lineH / 2;

    // Pack status list
    for (const ZipPackStatus& s : status) {
        const char* tag = (s.zipValid ? "[FOUND]  " : "[MISSING]");
        std::string line = std::string(tag) + " " + s.pack.filename;
        if (s.pack.mandatory) line += " *";
        SFont_Write(surf, bodyFont, 2, y, line.c_str());
        y += lineH;
    }

    y += lineH / 2;

    // Instructions
    if (canStart) {
        SFont_WriteCenter(surf, bodyFont, y, "ENTER or A  - Start game");
        y += lineH;
    }
    SFont_WriteCenter(surf, bodyFont, y, "F or X  - Browse for folder");
    y += lineH;
    SFont_WriteCenter(surf, bodyFont, y, "Q or B  - Quit");
    y += lineH;

    if (!canStart) {
        y += lineH / 2;
        SFont_WriteCenter(surf, bodyFont, y, "* required to play");
    }
}

// ---------------------------------------------------------------------------
// ConfigScreen::Run
// ---------------------------------------------------------------------------

fs::path ConfigScreen::Run(SDL_Renderer* renderer, SDL_Joystick* /*joystick*/) {
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

    // Body font: lemon.png
    SDL_Surface* lemonSurf = LoadSurfaceFromMemory(embedded_lemon_png, embedded_lemon_png_len);
    if (!lemonSurf) {
        SFont_FreeFont(headingFont);
        LOG_WARN("ConfigScreen: failed to load embedded lemon font");
        SDL_RenderSetLogicalSize(renderer, 0, 0);
        return {};
    }
    SFont_Font* bodyFont = SFont_InitFont(lemonSurf);
    if (!bodyFont) {
        SDL_FreeSurface(lemonSurf);
        SFont_FreeFont(headingFont);
        LOG_WARN("ConfigScreen: failed to init lemon SFont");
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
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_KP_ENTER:
                        confirmStart();
                        break;
                    default:
                        break;
                }
            }
            if (e.type == SDL_JOYBUTTONDOWN) {
                int btn = e.jbutton.button;
                if (btn == 0 || btn == 7 || btn == 9) {
                    // A / Start button: start if possible, else browse
                    if (canStart) confirmStart();
                    else browseForFolder();
                } else if (btn == 1) {
                    // B button: quit
                    running = false;
                } else if (btn == 2 || btn == 3) {
                    // X / Y button: browse
                    browseForFolder();
                }
            }
        }

        // Render
        DrawScreen(surf, headingFont, bodyFont, packStatus, currentDir, canStart);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, tex, nullptr, nullptr);
            SDL_DestroyTexture(tex);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);  // ~60 fps, no vsync needed here
    }

    SDL_FreeSurface(surf);
    SFont_FreeFont(bodyFont);
    SFont_FreeFont(headingFont);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    return result;
}
