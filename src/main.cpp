#include "SDL_log.h"
#include "core/log.h"
#include "core/global.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/mouse.h"
#include "core/gamepad.h"
#include "parsers/libp.h"
#include "core/state.h"
#include "core/save_file.h"
#include "scenes/scene.h"
#include "debug/debug_overlay.h"
#include "debug/debug_ui.h"
#include "debug/main_debug.h"
#include "tinyfiledialogs.h"
#include "core/embedded_loader.h"
#include "scenes/config_screen.h"
#include "race/race_sim.h"
#include "core/utils.h"
#include <filesystem>
#include <fstream>
#include <cstring>

// Embed resources for some small data files
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

// speed in pixels/second
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
    // Method 1: Check Steam Deck specific environment variables
    const char* steamDeck = getenv("STEAM_DECK");
    if (steamDeck && strcmp(steamDeck, "1") == 0) {
        return true;
    }

    // Method 2: Check for Steam Deck specific hardware
    const char* hardware = getenv("HARDWARE");
    if (hardware && strcmp(hardware, "steamdeck") == 0) {
        return true;
    }

    // Method 3: Check for Steam Deck specific platform
    const char* platform = getenv("PLATFORM");
    if (platform && strcmp(platform, "steamdeck") == 0) {
        return true;
    }

    // Method 4: Check for Steam Deck specific device model
    const char* deviceModel = getenv("DEVICE_MODEL");
    if (deviceModel && strstr(deviceModel, "steamdeck") != nullptr) {
        return true;
    }

    // Method 5: Check for Steam Deck specific user agent
    const char* userAgent = getenv("HTTP_USER_AGENT");
    if (userAgent && strstr(userAgent, "SteamDeck") != nullptr) {
        return true;
    }

    // Method 6: Check for Steam Deck specific file paths
    if (fs::exists("/sys/devices/virtual/dmi/id/product_name")) {
        std::ifstream productFile("/sys/devices/virtual/dmi/id/product_name");
        std::string productName;
        if (std::getline(productFile, productName)) {
            if (productName.find("Steam Deck") != std::string::npos) {
                return true;
            }
        }
    }

    // Method 7: Check for Steam Deck specific joystick/gamepad. Only meaningful
    // once the joystick subsystem is up, so this runs after SDL_Init.
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

    // Check cooldown
    if (currentTime - lastScreenshotTime < SCREENSHOT_COOLDOWN_MS) {
        return; // Still in cooldown period
    }

    lastScreenshotTime = currentTime;

    // Get the actual window size
    int windowWidth, windowHeight;
    SDL_GetWindowSize(win, &windowWidth, &windowHeight);

    // Create a surface to hold the pixel data
    sdl2::SurfacePtr surface(SDL_CreateRGBSurface(0, windowWidth, windowHeight, 32,
                                               0x00FF0000,  // Red mask
                                               0x0000FF00,  // Green mask
                                               0x000000FF,  // Blue mask
                                               0xFF000000)); // Alpha mask

    if (!surface) {
        LOG_INFO("Failed to create surface for screenshot: %s", SDL_GetError());
        return;
    }

    // Read pixels from the renderer
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                            surface->pixels, surface->pitch) != 0) {
        LOG_INFO("Failed to read pixels for screenshot: %s", SDL_GetError());
        return;
    }

    // Find next available counter value
    while (fs::exists("screenshot_" + std::to_string(screenshotCounter) + ".png")) {
        screenshotCounter++;
    }

    // Generate filename with counter
    std::string filename = "screenshot_" + std::to_string(screenshotCounter++) + ".png";

    // Save the surface as PNG
    if (IMG_SavePNG(surface.get(), filename.c_str()) != 0) {
        LOG_INFO("Failed to save screenshot: %s", SDL_GetError());
    } else {
        LOG_INFO("Screenshot saved: %s", filename.c_str());
    }

    // RAII handles cleanup automatically
}


#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = 1;
    char* argv[] = { "rodlegacy.exe", nullptr };
#else

int main(int argc, char* argv[]) {
#endif

    bool debugMode = false;
    bool muteAudio = false;
    bool forceConfig = false;
    bool selfTestSave = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else if (strcmp(argv[i], "--original-size") == 0) {
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
            printf("  --debug          Enable debug mode\n");
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

    // Attempt to initialize graphics and timer system
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

    // Resizable only with --debug: the debug UI wants room to drag panels
    // around, while the plain game keeps its fixed presentation.
    const Uint32 windowFlags = debugMode ? SDL_WINDOW_RESIZABLE : 0;
    win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           windowWidth, windowHeight, windowFlags);
    if (!win) {
        LOG_INFO("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Renderer hints only take effect if they are set before the renderer is
    // created, so they belong here rather than next to SDL_RenderSetIntegerScale.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // Nearest neighbor scaling
    if (useOriginalSize) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");  // Pixel-perfect at 1x
        LOG_INFO("Using software renderer for original size mode");
    }

    // Create renderer with proper flags
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

    // tinyfiledialogs doesn't require initialization

#ifndef _WIN32
    // Load icon from embedded data (only on non-Windows platforms)
    sdl2::SurfacePtr icon(LoadSurfaceFromMemory(embedded_icon_png, embedded_icon_png_len));
    if (!icon) {
        LOG_INFO("Could not load embedded icon");
    } else {
        SDL_SetWindowIcon(win, icon.get());
        // RAII handles cleanup automatically
    }
#endif

    if (!useOriginalSize) {
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);    // Force integer scaling only for upscaled mode
    }

    // Initialize SDL2_mixer
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        LOG_INFO("error opening audio: %s\n", Mix_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    //Mix_Music *music = Mix_LoadMUS(TUNE);
    //Mix_PlayMusic(music, 1);

    // Gamepad
    gamepad.OpenFirst();

    // Check if running on Steam Deck (needs the joystick subsystem, so not earlier)
    if (IsSteamDeck()) {
        LOG_INFO("Steam Deck detected - optimizing for handheld experience");
        // Enable gamepad mode by default on Steam Deck
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

    // Now that we have a valid game path, initialize the game state
    try {
        SDL_ShowCursor(SDL_DISABLE);
        state.Init(debugMode, gamePath, muteAudio);
        mouse.loadPointers(state.GetAssets().GetMousePointers());
        mouse.SetPointerType(SrLibAmigaSpriteAssetMousePointerHand);
        state.MoveToScene(SCENE_NAME_ENTRY);
        if (selfTestSave) return RunSaveRoundTripSelfTest() ? 0 : 1;
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
            Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Res scaling
    if (debugUi.Active()) {
        // The dockspace decides where the game is drawn, so SDL's logical-size
        // mapping would put mouse coordinates in the wrong place. The game
        // renders into a texture that is already exactly GAME_WIDTH x
        // GAME_HEIGHT, so no logical scaling is needed; coordinates are mapped
        // from the viewport rect instead (see RemapMouseToGameViewport).
        LOG_INFO("Debug UI active - logical scaling left off, dockspace owns layout");
    } else if (useOriginalSize) {
        // When using original size, no logical scaling - render directly to window
        LOG_INFO("Using original size - no logical scaling applied (1:1 pixel mapping)");
    } else {
        // When using upscaled window, set logical size to game resolution
        SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);
        LOG_INFO("Using upscaled window - logical size set to %dx%d", GAME_WIDTH, GAME_HEIGHT);
    }

    // Force integer scaling
    // SDL_RenderSetIntegerScale(rend, SDL_TRUE);


    // state.SetCurrentScene(&diner);

    // struct to hold the position and size of the sprite
    SDL_Rect dest;

    // get and scale the dimensions of texture
    SDL_QueryTexture(state.GetCurrentScene()->GetTexture(), NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    bool close_requested = false;

    // Delta-time tracking
    Uint64 prevTicks = SDL_GetTicks64();

    while (!close_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Quit must always get through, whatever the UI wants.
            if (debugUi.ProcessEvent(event) && event.type != SDL_QUIT) {
                continue;
            }
            if (debugUi.Active()) {
                RemapMouseToGameViewport(event, debugUi.GameViewportRect());
            }
            gamepad.HandleEvent(event);
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
                            case SDLK_F1:
                                if (event.type == SDL_KEYDOWN) {
                                    debugMode = !debugMode;
                                    LOG_INFO("Debug mode toggled: %s", debugMode ? "ENABLED" : "DISABLED");
                                    if (state.GetCurrentScene()) {
                                        state.GetCurrentScene()->Init(debugMode);
                                    }
                                }
                                break;
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

        // Calculate delta-time properly
        Uint64 now = SDL_GetTicks64();
        Uint64 delta = now - prevTicks;
        prevTicks = now;

        // Snapshot the pad once per frame, before anything reads it: the cursor
        // here and the driving scene below both see the same button edges.
        gamepad.Update();
        mouse.UpdateGamepad(state.GetCurrentScene(), delta);

        // Clear to black. Scenes (e.g. racing gauges/dot) leave the draw color
        // set to their last color, so reset it here or the clear tints everything.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        Scene *scene = state.GetCurrentScene();

        // Update state with delta-time in milliseconds
        state.SetElapsedTime(delta);

        // Update scene (handles fade transitions and other per-frame logic)
        scene->Update(delta);

        if (AudioManager* am = state.GetAssets().GetAudioManager()) {
            am->UpdateMusic();
        }

        // With the debug UI up the game renders into an offscreen texture that
        // the dockspace displays; otherwise it goes straight to the backbuffer.
        if (debugUi.Active()) {
            SDL_SetRenderTarget(renderer, debugUi.GameTexture());
            SDL_RenderClear(renderer);
        }

        scene->Draw();

        mouse.Draw();

        if (debugUi.Active()) {
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            debugUi.Draw(BuildDebugPanels(delta));
        }

        SDL_RenderPresent(renderer);

        // Frame rate limiting: rely on vsync primarily, but add delay as backup
        // Since we use SDL_RENDERER_PRESENTVSYNC, this delay is mainly for non-vsync scenarios
        if (delta < (1000 / GAME_FPS)) {
            SDL_Delay((1000 / GAME_FPS) - delta);
        }
    }

    // tinyfiledialogs doesn't require cleanup

    // Persist in-session settings changed via raw bool* bindings (e.g. the
    // debug "Show zone overlays" toggle) that have no per-change save hook.
    state.GetAssets().SaveSettings();

    debugUi.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}
