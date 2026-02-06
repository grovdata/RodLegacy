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

#define APP_TITLE "OpenSR"

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
        SDL_Log("Failed to create surface for screenshot: %s", SDL_GetError());
        return;
    }

    // Read pixels from the renderer
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                            surface->pixels, surface->pitch) != 0) {
        SDL_Log("Failed to read pixels for screenshot: %s", SDL_GetError());
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
        SDL_Log("Failed to save screenshot: %s", SDL_GetError());
    } else {
        SDL_Log("Screenshot saved: %s", filename.c_str());
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
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else if (strcmp(argv[i], "--original-size") == 0) {
            useOriginalSize = true;
        } else if (strcmp(argv[i], "--mute") == 0) {
            muteAudio = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --debug          Enable debug mode\n");
            printf("  --original-size  Run in original 320x200 resolution (default: 4x upscaled)\n");
            printf("  --mute           Mute all audio\n");
            printf("  --help, -h       Show this help message\n");
            return 0;
        }
    }

    // Check if running on Steam Deck
    bool isSteamDeck = IsSteamDeck();
    if (isSteamDeck) {
        SDL_Log("Steam Deck detected - optimizing for handheld experience");
        // Enable gamepad mode by default on Steam Deck
        mouse.SetGamepadActive(true);
    } else {
        SDL_Log("Running on desktop/laptop system");
    }

    SDL_Log("Debug mode: %s", debugMode ? "ENABLED" : "DISABLED");
    SDL_Log("Window size: %s", useOriginalSize ? "320x200 (original)" : "1280x800 (4x upscaled)");

    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        SDL_Log("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create window after SDL_Init
    int windowWidth = useOriginalSize ? ORIGINAL_WINDOW_WIDTH : WINDOW_WIDTH;
    int windowHeight = useOriginalSize ? ORIGINAL_WINDOW_HEIGHT : WINDOW_HEIGHT;

    SDL_Log("Creating window: %dx%d (%s)", windowWidth, windowHeight,
            useOriginalSize ? "original size" : "4x upscaled");

    win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, 0);
    if (!win) {
        SDL_Log("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer with proper flags
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // tinyfiledialogs doesn't require initialization

#ifndef _WIN32
    // Load icon from embedded data (only on non-Windows platforms)
    sdl2::SurfacePtr icon(LoadSurfaceFromMemory(embedded_icon_png, embedded_icon_png_len));
    if (!icon) {
        SDL_Log("Could not load embedded icon");
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
        SDL_Log("Using software renderer for original size mode");
    } else {
        SDL_RenderSetIntegerScale(renderer, SDL_TRUE);    // Force integer scaling only for upscaled mode
    }

    // Initialize SDL2_mixer
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        printf(
            "SDL2_mixer could not be initialized!\n"
            "SDL_Error: %s\n",
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
            printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
        }
    }


    fs::path gamePath;
    try {
        gamePath = AssetManager::FindGameDirectory();
    } catch (const AssetLoadError& e) {
        const char* selectedPath = tinyfd_selectFolderDialog("Select game data folder", nullptr);

        if (selectedPath != nullptr) {
            fs::path gameDataPath = fs::path(selectedPath);
            if (AssetManager::ValidateGamePath(gameDataPath)) {
                gamePath = gameDataPath;
                AssetManager::SaveGamePath(gameDataPath);
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error",
                    "Failed to load game files from selected folder.\nPlease ensure SR1AMIGA_DISK1.adf and SR1AMIGA_DISK2.adf are present.", win);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(win);
                SDL_JoystickClose(controller);
                Mix_CloseAudio();
                SDL_Quit();
                return 1;
            }
        } else {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "No game files found",
                "Failed to find game files and no folder selected.\nSR1AMIGA_DISK1.adf and SR1AMIGA_DISK2.adf are required.", win);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(win);
            SDL_JoystickClose(controller);
            Mix_CloseAudio();
            SDL_Quit();
            return 1;
        }
    }

    // Now that we have a valid game path, initialize the game state
    try {
        SDL_ShowCursor(SDL_DISABLE);
        state.Init(debugMode, gamePath, muteAudio);
        mouse.loadPointers(state.GetAssets().GetMousePointers());
        mouse.SetPointerType(SrLibAmigaSpriteAssetMousePointerHand);
        state.MoveToScene(SCENE_GARAGE);
    } catch (const std::exception& e) {
        SDL_Log("Failed to initialize game state: %s", e.what());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_JoystickClose(controller);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Also ensure initial scene is set up for normal initialization path
    if (!state.GetCurrentScene()) {
        state.MoveToScene(SCENE_GARAGE);
    }

    // Res scaling
    if (useOriginalSize) {
        // When using original size, no logical scaling - render directly to window
        SDL_Log("Using original size - no logical scaling applied (1:1 pixel mapping)");
    } else {
        // When using upscaled window, set logical size to game resolution
        SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);
        SDL_Log("Using upscaled window - logical size set to %dx%d", GAME_WIDTH, GAME_HEIGHT);
    }

    // Force integer scaling
    // SDL_RenderSetIntegerScale(rend, SDL_TRUE);

    state.MoveToScene(SCENE_GARAGE);

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
            state.GetCurrentScene()->PushEvent(&event);
            switch (event.type) {

                case SDL_QUIT:
                    close_requested = true;
                    SDL_Log("Quitting");
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_F1:
                            // F1 toggles debug mode
                            debugMode = !debugMode;
                            SDL_Log("Debug mode toggled: %s", debugMode ? "ENABLED" : "DISABLED");
                            // Reinitialize the current scene with new debug mode
                            if (state.GetCurrentScene()) {
                                state.GetCurrentScene()->Init(debugMode);
                            }
                            break;
                        case SDLK_LEFT:
                            break;
                        case SDLK_RIGHT:
                            break;
                        case SDLK_F5:
                            TakeScreenshot();
                            break;

                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
                        case SDLK_g:
                            if ((event.key.keysym.mod & KMOD_CTRL) != 0) {
                                // Ctrl+G toggles gamepad mode
                                mouse.SetGamepadActive(!mouse.IsGamepadActive());
                                SDL_Log("Gamepad mode: %s", mouse.IsGamepadActive() ? "ON" : "OFF");
                            } else {
                                state.MoveToScene(SCENE_GARAGE);
                            }
                            break;
                        case SDLK_d:
                            state.MoveToScene(SCENE_DINER);
                            break;
                        case SDLK_f:
                            if ((event.key.keysym.mod & KMOD_CTRL) != 0) {
                                ToggleFullScreen(win, isFullScreen);
                            }
                            break;
                        case SDLK_r:
                            state.MoveToScene(SCENE_DRIVING);
                            break;
                        case SDLK_ESCAPE:
                            close_requested = true;
                            break;
                        case SDLK_LEFT:
                            break;
                        case SDLK_RIGHT:
                            break;
                        case SDLK_F5:
                            TakeScreenshot();
                            break;

                        default:
                            break;
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
        //     SDL_Log("Adding gamepad debug info to scene");
        //     state.GetCurrentScene()->AddGamepadDebugInfo(mouse);
        // } else {
        //     SDL_Log("No current scene available for gamepad debug info");
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
        scene->DrawExtra();

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
