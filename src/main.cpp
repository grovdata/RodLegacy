#include "SDL_log.h"
#include "log.h"
#include "global.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "mouse.h"
#include "parsers/libp.h"
#include "state.h"
#include "scene.h"
#include "debug_overlay.h"
#include "tinyfiledialogs.h"
#include "embedded_loader.h"
#include "config_screen.h"
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <cstring>

// Generated embedded resources
extern "C" {
    #include "embedded_resources.h"
}

namespace fs = std::filesystem;

#define BITPLANE_DEPTH 4

#define WINDOW_WIDTH (GAME_WIDTH * 4)
#define WINDOW_HEIGHT (GAME_HEIGHT * 4)
#define ORIGINAL_WINDOW_WIDTH GAME_WIDTH
#define ORIGINAL_WINDOW_HEIGHT GAME_HEIGHT

#define APP_TITLE "RodLegacy"

// speed in pixels/second
#define SPEED (300)

SDL_Window *win = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Joystick *controller = NULL;
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

    // Method 7: Check for Steam Deck specific joystick/gamepad
    if (SDL_NumJoysticks() > 0) {
        const char* joystickName = SDL_JoystickNameForIndex(0);
        if (joystickName && strstr(joystickName, "Steam Deck") != nullptr) {
            return true;
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
    char* argv[] = { "open_sr.exe", nullptr };
#else
int main(int argc, char* argv[]) {
#endif

    bool debugMode = false;
    bool muteAudio = false;
    bool forceConfig = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else if (strcmp(argv[i], "--original-size") == 0) {
            useOriginalSize = true;
        } else if (strcmp(argv[i], "--mute") == 0) {
            muteAudio = true;
        } else if (strcmp(argv[i], "-config") == 0) {
            forceConfig = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --debug          Enable debug mode\n");
            printf("  --original-size  Run in original 320x200 resolution (default: 4x upscaled)\n");
            printf("  --mute           Mute all audio\n");
            printf("  -config          Show setup screen even if game files are already configured\n");
            printf("  --help, -h       Show this help message\n");
            return 0;
        }
    }

    // Check if running on Steam Deck
    bool isSteamDeck = IsSteamDeck();
    if (isSteamDeck) {
        LOG_INFO("Steam Deck detected - optimizing for handheld experience");
        // Enable gamepad mode by default on Steam Deck
        mouse.SetGamepadActive(true);
    } else {
        LOG_INFO("Running on desktop/laptop system");
    }

    LOG_INFO("Debug mode: %s", debugMode ? "ENABLED" : "DISABLED");
    LOG_INFO("Window size: %s", useOriginalSize ? "320x200 (original)" : "1280x800 (4x upscaled)");

    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        LOG_INFO("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Set log priority: default to INFO, verbose in debug mode
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN);
    if (debugMode) {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
    }

    // Create window after SDL_Init
    int windowWidth = useOriginalSize ? ORIGINAL_WINDOW_WIDTH : WINDOW_WIDTH;
    int windowHeight = useOriginalSize ? ORIGINAL_WINDOW_HEIGHT : WINDOW_HEIGHT;

    LOG_INFO("Creating window: %dx%d (%s)", windowWidth, windowHeight,
            useOriginalSize ? "original size" : "4x upscaled");

    win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, 0);
    if (!win) {
        LOG_INFO("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer with proper flags
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        LOG_INFO("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
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

    // Set up renderer for pixel-perfect rendering
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // Nearest neighbor scaling
    if (useOriginalSize) {
        // For original size, disable all scaling hints
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");  // Use software renderer for pixel-perfect rendering
        LOG_INFO("Using software renderer for original size mode");
    } else {
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);    // Force integer scaling only for upscaled mode
    }

    // Initialize SDL2_mixer
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        LOG_WARN("SDL2_mixer could not be initialized! SDL_Error: %s",
            SDL_GetError());
        return 0;
    }
    //Mix_Music *music = Mix_LoadMUS(TUNE);
    //Mix_PlayMusic(music, 1);

    // Gamepad / joystick
    if (SDL_NumJoysticks() > 0) {
        // Load joystick
        controller = SDL_JoystickOpen(0);
        if (controller == NULL) {
            LOG_WARN("Unable to open game controller! SDL Error: %s", SDL_GetError());
        }
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
        gamePath = ConfigScreen::Run(renderer, controller);
        if (gamePath.empty()) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(win);
            SDL_JoystickClose(controller);
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
        state.MoveToScene(SCENE_GARAGE);
    } catch (const std::exception& e) {
        LOG_INFO("Failed to initialize game state: %s", e.what());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_JoystickClose(controller);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Res scaling
    if (useOriginalSize) {
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
                                if (event.type == SDL_KEYUP) {
                                    if ((event.key.keysym.mod & KMOD_CTRL) != 0) {
                                        mouse.SetGamepadActive(!mouse.IsGamepadActive());
                                        LOG_INFO("Gamepad mode: %s", mouse.IsGamepadActive() ? "ON" : "OFF");
                                    } else {
                                        state.MoveToScene(SCENE_GARAGE);
                                    }
                                }
                                break;
                            case SDLK_d:
                                if (event.type == SDL_KEYUP) {
                                    state.MoveToScene(SCENE_DINER);
                                }
                                break;
                            case SDLK_f:
                                if (event.type == SDL_KEYUP && (event.key.keysym.mod & KMOD_CTRL) != 0) {
                                    ToggleFullScreen(win, isFullScreen);
                                }
                                break;
                            case SDLK_r:
                                if (event.type == SDL_KEYUP) {
                                    state.MoveToScene(SCENE_DRIVING);
                                }
                                break;
                            case SDLK_e:
                                if (event.type == SDL_KEYUP) {
                                    state.MoveToScene(SCENE_KING_OF_COUNTY);
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
                case SDL_JOYAXISMOTION:
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                case SDL_JOYHATMOTION:
                    mouse.Update(&event, state.GetCurrentScene());
                    break;
                default:
                    break;
            }
        }

        // Update gamepad input (separate from event polling)
        mouse.UpdateGamepad(state.GetCurrentScene());

        // Add gamepad debug info to scene
        // if (state.GetCurrentScene()) {
        //     LOG_INFO("Adding gamepad debug info to scene");
        //     state.GetCurrentScene()->AddGamepadDebugInfo(mouse);
        // } else {
        //     LOG_INFO("No current scene available for gamepad debug info");
        // }

        SDL_RenderClear(renderer);

        Scene *scene = state.GetCurrentScene();

        // Calculate delta-time properly
        Uint64 now = SDL_GetTicks64();
        Uint64 delta = now - prevTicks;
        prevTicks = now;

        // Update state with delta-time in milliseconds
        state.SetElapsedTime(delta);

        // Update scene (handles fade transitions and other per-frame logic)
        scene->Update(delta);

        scene->Draw();

        mouse.Draw();

        SDL_RenderPresent(renderer);

        // Frame rate limiting: rely on vsync primarily, but add delay as backup
        // Since we use SDL_RENDERER_PRESENTVSYNC, this delay is mainly for non-vsync scenarios
        if (delta < (1000 / GAME_FPS)) {
            SDL_Delay((1000 / GAME_FPS) - delta);
        }
    }

    //printf("Whadda loser!\n");

    // delete currentScene;

    // tinyfiledialogs doesn't require cleanup

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_JoystickClose(controller);
    Mix_CloseAudio();
    SDL_Quit();

}
