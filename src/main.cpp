#include "SDL_log.h"
#include "core/log.h"
#include "core/global.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/mouse.h"
#include "core/gamepad.h"
#include "gfx/bitmap.h"
#include "ui/control_icons.h"
#include "parsers/libp.h"
#include "core/state.h"
#include "core/save_file.h"
#include "scenes/scene.h"
#include "debug/debug_overlay.h"
#include "debug/debug_ui.h"
#include "debug/main_debug.h"
#include "core/embedded_loader.h"
#include "scenes/config_screen.h"
#include "race/race_sim.h"
#include "core/utils.h"
#include <filesystem>
#include <fstream>
#include <cstring>

extern "C" {
    #include "embedded_resources.h"
}

namespace fs = std::filesystem;

#define BITPLANE_DEPTH 4

#define WINDOW_WIDTH (GAME_WIDTH * 4)
#define WINDOW_HEIGHT (GAME_HEIGHT * 4)
#define ORIGINAL_WINDOW_WIDTH GAME_WIDTH
#define ORIGINAL_WINDOW_HEIGHT GAME_HEIGHT

#ifndef RODLEGACY_VERSION
#define RODLEGACY_VERSION "dev"
#endif

#define APP_TITLE "RodLegacy " RODLEGACY_VERSION

#define SPEED (300)

SDL_Window *win = nullptr;
SDL_Renderer *renderer = nullptr;
Gamepad gamepad;
Uint64 startTime = SDL_GetTicks64();

Mouse mouse;
bool isFullScreen = false;
bool useOriginalSize = false;
int screenshotCounter = 1;
Uint64 lastScreenshotTime = 0;
const Uint64 SCREENSHOT_COOLDOWN_MS = 500; // 500ms cooldown

float scaleX = 1.0f;
float scaleY = 1.0f;

bool IsSteamDeck() {
    const char* steamDeck = getenv("STEAM_DECK");
    if (steamDeck && strcmp(steamDeck, "1") == 0) {
        return true;
    }

    const char* hardware = getenv("HARDWARE");
    if (hardware && strcmp(hardware, "steamdeck") == 0) {
        return true;
    }

    const char* platform = getenv("PLATFORM");
    if (platform && strcmp(platform, "steamdeck") == 0) {
        return true;
    }

    const char* deviceModel = getenv("DEVICE_MODEL");
    if (deviceModel && strstr(deviceModel, "steamdeck") != nullptr) {
        return true;
    }

    const char* userAgent = getenv("HTTP_USER_AGENT");
    if (userAgent && strstr(userAgent, "SteamDeck") != nullptr) {
        return true;
    }

    if (fs::exists("/sys/devices/virtual/dmi/id/product_name")) {
        std::ifstream productFile("/sys/devices/virtual/dmi/id/product_name");
        std::string productName;
        if (std::getline(productFile, productName)) {
            if (productName.find("Steam Deck") != std::string::npos) {
                return true;
            }
        }
    }

    // Needs the joystick subsystem, so this runs after SDL_Init.
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            const char* joystickName = SDL_JoystickNameForIndex(i);
            if (joystickName && strstr(joystickName, "Steam Deck") != nullptr) {
                return true;
            }
        }
    }

    return false;
}

void ToggleFullScreen(SDL_Window* window, bool currentState) {
    isFullScreen = !currentState;
    SDL_SetWindowFullscreen(window, isFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void TakeScreenshot() {
    Uint64 currentTime = SDL_GetTicks64();

    if (currentTime - lastScreenshotTime < SCREENSHOT_COOLDOWN_MS) {
        return;
    }

    lastScreenshotTime = currentTime;

    int windowWidth, windowHeight;
    SDL_GetWindowSize(win, &windowWidth, &windowHeight);

    sdl2::SurfacePtr surface(SDL_CreateRGBSurface(0, windowWidth, windowHeight, 32,
                                               0x00FF0000,
                                               0x0000FF00,
                                               0x000000FF,
                                               0xFF000000));

    if (!surface) {
        LOG_INFO("Failed to create surface for screenshot: %s", SDL_GetError());
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                            surface->pixels, surface->pitch) != 0) {
        LOG_INFO("Failed to read pixels for screenshot: %s", SDL_GetError());
        return;
    }

    while (fs::exists("screenshot_" + std::to_string(screenshotCounter) + ".png")) {
        screenshotCounter++;
    }

    std::string filename = "screenshot_" + std::to_string(screenshotCounter++) + ".png";

    if (IMG_SavePNG(surface.get(), filename.c_str()) != 0) {
        LOG_INFO("Failed to save screenshot: %s", SDL_GetError());
    } else {
        LOG_INFO("Screenshot saved: %s", filename.c_str());
    }
}

// Global pause, owned by the loop: P on the keyboard, Start on the pad.
bool paused = false;
Uint64 pausedTicks = 0;   // total time spent paused
Uint64 pauseStarted = 0;  // when the current pause began

Uint64 GameTicks() {
    if (paused) return pauseStarted - pausedTicks;
    return SDL_GetTicks64() - pausedTicks;
}

bool IsPauseEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        // P belongs to a text field while one is active.
        return event.key.keysym.sym == SDLK_p &&
               (paused || SDL_IsTextInputActive() == SDL_FALSE);
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        return event.cbutton.button == SDL_CONTROLLER_BUTTON_START;
    }
    return false;
}

void SetPaused(bool value) {
    if (value == paused) return;
    paused = value;
    if (paused) {
        pauseStarted = SDL_GetTicks64();
    } else {
        pausedTicks += SDL_GetTicks64() - pauseStarted;
    }
    if (AudioManager* am = state.GetAssets().GetAudioManager()) {
        if (paused) {
            am->PauseAll();
        } else {
            am->ResumeAll();
        }
    }
    LOG_INFO("Pause: %s", paused ? "ON" : "OFF");
}

SDL_Surface* BuildPauseSurface(SFont_Font* font) {
    const char* kTitle = "PAUSED";
    const char* kHintHead = "Press P or ";
    const char* kHintTail = " to unpause";

    // A white cap with black lettering so it reads against the dim.
    SDL_Surface* icon = icons::Strip({{icons::Sheet::Controller, 14}},
                                     SDL_Color{0xFF, 0xFF, 0xFF, 0xFF},
                                     SDL_Color{0x00, 0x00, 0x00, 0xFF});

    const int lineH = SFont_TextHeight(font);
    const int iconW = icon ? icon->w : 0;
    const int iconH = icon ? icon->h : 0;
    const int hintH = std::max(lineH, iconH);
    const int hintW = SFont_TextWidth(font, kHintHead) + iconW +
                      SFont_TextWidth(font, kHintTail);
    const int gap = 6;

    // Same masks as Label.
    SDL_Surface* s = SDL_CreateRGBSurface(0, GAME_WIDTH, lineH + gap + hintH, 32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF,
                                          0xFF000000);
    if (!s) {
        if (icon) SDL_FreeSurface(icon);
        return nullptr;
    }
    SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, 0, 0, 0, 0));

    SFont_Write(s, font, (GAME_WIDTH - SFont_TextWidth(font, kTitle)) / 2, 0, kTitle);

    const int textY = lineH + gap + (hintH - lineH) / 2;
    int x = (GAME_WIDTH - hintW) / 2;
    SFont_Write(s, font, x, textY, kHintHead);
    x += SFont_TextWidth(font, kHintHead);
    if (icon) {
        SDL_Rect dst = {x, lineH + gap + (hintH - iconH) / 2, iconW, iconH};
        // Verbatim with alpha: blending onto a transparent surface loses the rounded corners.
        SDL_SetSurfaceBlendMode(icon, SDL_BLENDMODE_NONE);
        SDL_BlitSurface(icon, nullptr, s, &dst);
        SDL_FreeSurface(icon);
        x += iconW;
    }
    SFont_Write(s, font, x, textY, kHintTail);
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);  // LoadSurface copies by blit
    return s;
}

void DrawPauseOverlay() {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    const SDL_Rect full = {0, 0, GAME_WIDTH, GAME_HEIGHT};
    SDL_RenderFillRect(renderer, &full);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Built on first pause and never freed: its texture must not outlive the renderer.
    static Bitmap* overlay = nullptr;
    if (!overlay) {
        SFont_Font* font = state.GetAssets().GetMainFont();
        if (!font) return;
        SDL_Surface* s = BuildPauseSurface(font);
        if (!s) return;
        overlay = new Bitmap();
        overlay->LoadSurface(s);
        overlay->SetPosition(0, (GAME_HEIGHT - s->h) / 2);
        overlay->SetVisible(true);
        SDL_FreeSurface(s);
    }
    overlay->Draw();
}


#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // CRT globals, filled in under both the console and windows subsystems.
    int argc = __argc;
    char** argv = __argv;
#else

int main(int argc, char* argv[]) {
#endif

    bool debugMode = false;
    bool muteAudio = false;
    bool forceConfig = false;
    bool selfTestSave = false;
    for (int i = 1; i < argc; i++) {
#ifdef RODLEGACY_DEBUG
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else
#endif
        if (strcmp(argv[i], "--original-size") == 0) {
            useOriginalSize = true;
        } else if (strcmp(argv[i], "--mute") == 0) {
            muteAudio = true;
        } else if (strcmp(argv[i], "-config") == 0) {
            forceConfig = true;
        } else if (strcmp(argv[i], "--selftest-save") == 0) {
            selfTestSave = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("RodLegacy %s\n", RODLEGACY_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
#ifdef RODLEGACY_DEBUG
            printf("  --debug          Enable debug mode\n");
#endif
            printf("  --original-size  Run in original 320x200 resolution (default: 4x upscaled)\n");
            printf("  --mute           Mute all audio\n");
            printf("  --selftest-save  Save/load round-trip check, then exit (0 = pass)\n");
            printf("  -config          Show setup screen\n");
            printf("  --version        Print version and exit\n");
            printf("  --help, -h       Show this help message\n");
            return 0;
        }
    }
    LOG_INFO("Debug mode: %s", debugMode ? "ENABLED" : "DISABLED");
    LOG_INFO("Window size: %s", useOriginalSize ? "320x200 (original)" : "1280x800 (4x upscaled)");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO |
                 SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_INFO("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN);
    if (debugMode) {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
    }

    int windowWidth = useOriginalSize ? ORIGINAL_WINDOW_WIDTH : WINDOW_WIDTH;
    int windowHeight = useOriginalSize ? ORIGINAL_WINDOW_HEIGHT : WINDOW_HEIGHT;

    LOG_INFO("Creating window: %dx%d (%s)", windowWidth, windowHeight,
            useOriginalSize ? "original size" : "4x upscaled");

    // Resizable only with --debug, for dragging debug panels.
    const Uint32 windowFlags = debugMode ? SDL_WINDOW_RESIZABLE : 0;
    win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           windowWidth, windowHeight, windowFlags);
    if (!win) {
        LOG_INFO("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Renderer hints must be set before the renderer is created.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // Nearest neighbor scaling
    if (useOriginalSize) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");  // Pixel-perfect at 1x
        LOG_INFO("Using software renderer for original size mode");
    }

    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        LOG_INFO("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // Debug UI. Only built with -Ddebug_ui=true; without that option DebugUi is a
    // stub whose Active() is constant false, so everything below folds away.
    DebugUi debugUi;
    if (debugMode) {
        debugUi.Init(win, renderer, GAME_WIDTH, GAME_HEIGHT);
    }

#ifndef _WIN32
    sdl2::SurfacePtr icon(LoadSurfaceFromMemory(embedded_icon_png, embedded_icon_png_len));
    if (!icon) {
        LOG_INFO("Could not load embedded icon");
    } else {
        SDL_SetWindowIcon(win, icon.get());
    }
#endif

    if (!useOriginalSize) {
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);    // Force integer scaling only for upscaled mode
    }

    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        LOG_INFO("error opening audio: %s\n", Mix_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    gamepad.OpenFirst();

    // SDL enables text input by default on desktop; start off and let text fields turn it on.
    SDL_StopTextInput();

    if (IsSteamDeck()) {
        LOG_INFO("Steam Deck detected - optimizing for handheld experience");
        mouse.SetGamepadActive(true);
    } else {
        LOG_INFO("Running on desktop/laptop system");
    }


    fs::path gamePath;
    bool needsConfig = forceConfig;
    if (!needsConfig) {
        try {
            gamePath = AssetManager::FindGameDirectory();
        } catch (const AssetLoadError&) {
            needsConfig = true;
        }
    }

    if (needsConfig) {
        gamePath = RunConfigScreen(renderer);
        if (gamePath.empty()) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(win);
            Mix_CloseAudio();
            SDL_Quit();
            return 0;
        }
        AssetManager::SaveGamePath(gamePath);
        LOG_INFO("Game data: %s (config screen)", gamePath.string().c_str());
    }

    try {
        SDL_ShowCursor(SDL_DISABLE);
        state.Init(debugMode, gamePath, muteAudio);
        mouse.loadPointers(state.GetAssets().GetMousePointers());
        mouse.SetPointerType(SrLibAmigaSpriteAssetMousePointerHand);
        state.MoveToScene(state.GetAssets().GetSkipIntroRef() || selfTestSave
                              ? SCENE_NAME_ENTRY : SCENE_INTRO);
        if (selfTestSave) return RunSaveRoundTripSelfTest() ? 0 : 1;
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
            Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    if (debugUi.Active()) {
        // The dockspace decides where the game is drawn; mouse coordinates come from the viewport rect instead.
        LOG_INFO("Debug UI active - logical scaling left off, dockspace owns layout");
    } else if (useOriginalSize) {
        LOG_INFO("Using original size - no logical scaling applied (1:1 pixel mapping)");
    } else {
        SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);
        LOG_INFO("Using upscaled window - logical size set to %dx%d", GAME_WIDTH, GAME_HEIGHT);
    }


    SDL_Rect dest;

    SDL_QueryTexture(state.GetCurrentScene()->GetTexture(), NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    bool close_requested = false;

    Uint64 prevTicks = SDL_GetTicks64();

    while (!close_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Quit must always get through.
            if (debugUi.ProcessEvent(event) && event.type != SDL_QUIT) {
                continue;
            }
            if (debugUi.Active()) {
                RemapMouseToGameViewport(event, debugUi.GameViewportRect());
            }
            gamepad.HandleEvent(event);

            // While paused only quitting gets through.
            if (IsPauseEvent(event)) {
                if ((event.type == SDL_KEYDOWN && event.key.repeat == 0) ||
                    event.type == SDL_CONTROLLERBUTTONDOWN) {
                    SetPaused(!paused);
                }
                continue;
            }
            if (paused && event.type != SDL_QUIT) {
                continue;
            }

            bool consumed = state.GetCurrentScene()->PushEvent(&event);
            switch (event.type) {
                case SDL_QUIT:
                    close_requested = true;
                    LOG_INFO("Quitting");
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    if (!consumed) {
                        switch (event.key.keysym.sym) {
#ifdef RODLEGACY_DEBUG
                            case SDLK_F1:
                                if (event.type == SDL_KEYDOWN) {
                                    debugMode = !debugMode;
                                    LOG_INFO("Debug mode toggled: %s", debugMode ? "ENABLED" : "DISABLED");
                                    if (state.GetCurrentScene()) {
                                        state.GetCurrentScene()->Init(debugMode);
                                    }
                                }
                                break;
#endif
                            case SDLK_F5:
                                if (event.type == SDL_KEYUP) {
                                    TakeScreenshot();
                                }
                                break;
                            case SDLK_g:
                                if (event.type == SDL_KEYUP && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                                    mouse.SetGamepadActive(!mouse.IsGamepadActive());
                                    LOG_INFO("Gamepad mode: %s", mouse.IsGamepadActive() ? "ON" : "OFF");
                                }
                                break;
                            case SDLK_f:
                                if (event.type == SDL_KEYUP && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                                    ToggleFullScreen(win, isFullScreen);
                                }
                                break;
                            case SDLK_F11:
                                if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                                    ToggleFullScreen(win, isFullScreen);
                                }
                                break;
                            case SDLK_ESCAPE:
                                if (event.type == SDL_KEYDOWN) {
                                    close_requested = true;
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONUP:
                case SDL_CONTROLLERAXISMOTION:
                case SDL_CONTROLLERBUTTONDOWN:
                case SDL_CONTROLLERBUTTONUP:
                    mouse.Update(&event, state.GetCurrentScene());
                    break;
                default:
                    break;
            }
        }

        Uint64 now = SDL_GetTicks64();
        Uint64 delta = now - prevTicks;
        prevTicks = now;

        // Once per frame, before anything reads it.
        gamepad.Update();
        if (!paused) {
            mouse.UpdateGamepad(state.GetCurrentScene(), delta);
        }

        // Scenes leave the draw colour set, which would tint the clear.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        Scene *scene = state.GetCurrentScene();

        state.SetElapsedTime(paused ? 0 : delta);

        if (!paused) {
            scene->Update(delta);

            if (AudioManager* am = state.GetAssets().GetAudioManager()) {
                am->UpdateMusic();
            }
        }

        // With the debug UI up the game renders into an offscreen texture.
        if (debugUi.Active()) {
            SDL_SetRenderTarget(renderer, debugUi.GameTexture());
            SDL_RenderClear(renderer);
        }

        scene->Draw();

        if (paused) {
            DrawPauseOverlay();
        }

        mouse.Draw();

        if (debugUi.Active()) {
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            debugUi.Draw(BuildDebugPanels(delta));
        }

        SDL_RenderPresent(renderer);

        // Backup for when vsync is unavailable.
        if (delta < (1000 / GAME_FPS)) {
            SDL_Delay((1000 / GAME_FPS) - delta);
        }
    }


    // Raw bool* bindings have no per-change save hook.
    state.GetAssets().SaveSettings();

    debugUi.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}
