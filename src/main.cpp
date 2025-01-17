#include "global.h"
#include "mouse.h"
#include "parsers/libp.h"
#include "state.h"
#include "scene.h"
#include "debug_overlay.h"
#include <nfd.h>
#include <nfd_sdl2.h>
#include <filesystem>

namespace fs = std::filesystem;

#define BITPLANE_DEPTH 4

#define WINDOW_WIDTH (GAME_WIDTH * 4)
#define WINDOW_HEIGHT (GAME_HEIGHT * 4)

#define FONT "../data/font/font.png"
#define DEBUG_FONT "../data/font/lemon.png"
#define TUNE "../data/snd/sr1_ost.ogg"

#define ICON "../data/res/icon.png"
#define APP_TITLE "OpenSR"

// speed in pixels/second
#define SPEED (300)

SDL_Window *win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SDL_Joystick *controller = NULL;
Uint64 startTime = SDL_GetTicks64();

Mouse mouse;
bool isFullScreen = false;

float scaleX = 1.0f;
float scaleY = 1.0f;

void ToggleFullScreen(SDL_Window* window, bool currentState) {
    isFullScreen = !currentState;
    int r = SDL_SetWindowFullscreen(window, isFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(currentState);
}

int main(int argc, char *argv[]) {

    bool debugMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        }
    }

    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        SDL_Log("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (NFD_Init() != NFD_OKAY) {
        SDL_Log("error initializing NFD: %s\n", NFD_GetError());
        SDL_Quit();
        return 1;
    }

    if (!win) {
        SDL_Log("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Surface *icon = IMG_Load(ICON);
    if (icon == NULL) {
        printf("Could not load icon");
    } else {
        SDL_SetWindowIcon(win, icon);
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    // Initialize SDL2_mixer
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        printf(
            "SDL2_mixer could not be initialized!\n"
            "SDL_Error: %s\n",
            SDL_GetError());
        return 0;
    }
    Mix_Music *music = Mix_LoadMUS(TUNE);
    //Mix_PlayMusic(music, 1);

    // Gamepad / joystick
    if (SDL_NumJoysticks() > 0) {
        // Load joystick
        controller = SDL_JoystickOpen(0);
        if (controller == NULL) {
            printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
        }
    }

    if (!renderer) {
        printf("error creating renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    fs::path gamePath;
    try {
        gamePath = AssetManager::FindGameDirectory();
    } catch (const AssetLoadError& e) {
        nfdu8char_t *outPath = NULL;
        nfdpickfolderu8args_t args = {0};
        nfdresult_t result = NFD_PickFolderU8_With(&outPath, &args);
            
        if (result == NFD_OKAY) {
            fs::path selectedPath = fs::path(outPath);
            if (AssetManager::ValidateGamePath(selectedPath)) {
                gamePath = selectedPath;
                NFD_FreePathU8(outPath);
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", 
                    "Failed to load game files from selected folder.\nPlease ensure SR1AMIGA_DISK1.adf and SR1AMIGA_DISK2.adf are present.", win);
                NFD_FreePathU8(outPath);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(win);
                SDL_JoystickClose(controller);
                Mix_FreeMusic(music);
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
            Mix_FreeMusic(music);
            Mix_CloseAudio();
            SDL_Quit();
            return 1;
        }
    }

    // Now that we have a valid game path, initialize the game state
    try {
        state.Init(debugMode, gamePath);
        mouse.loadPointers(state.assets.GetMousePointers());        
        mouse.SetPointerType(SrLibAmigaSpriteAssetMousePointerHand);
        state.MoveToScene(SCENE_GARAGE);
    } catch (const std::exception& e) {
        SDL_Log("Failed to initialize game state: %s", e.what());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_JoystickClose(controller);
        Mix_FreeMusic(music);
        Mix_CloseAudio();
        SDL_Quit();
        return 1;
    }

    // Also ensure initial scene is set up for normal initialization path
    if (!state.GetCurrentScene()) {
        state.MoveToScene(SCENE_GARAGE);
    }

    // Res scaling
    SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);

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

    Uint64 timeDelta = 0;

  


    while (!close_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            state.GetCurrentScene()->PushEvent(&event);
            switch (event.type) {

                case SDL_QUIT:
                    close_requested = true;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:
                            break;
                        case SDLK_RIGHT:
                            break;

                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
                        case SDLK_g:
                            state.MoveToScene(SCENE_GARAGE);
                            break;
                        case SDLK_d:
                            state.MoveToScene(SCENE_DINER);
                            break;
                        case SDLK_f:
                            if ((event.key.keysym.mod & KMOD_CTRL) != 0) {
                                ToggleFullScreen(win, isFullScreen);
                            }
                            break;
                        case SDLK_ESCAPE:
                            close_requested = true;
                            break;
                        case SDLK_LEFT:
                            break;
                        case SDLK_RIGHT:
                            break;

                        default:
                            break;
                    }
                    break;
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONUP:
                    mouse.Update(&event);
                    break;
                case SDL_JOYAXISMOTION:
                    // handle
                    break;
                default:
                    break;
            }
        }

        SDL_RenderClear(renderer);

        Scene *scene = state.GetCurrentScene();


        scene->Draw();

        // This is hacky, we can do better
        SrLibGameScene gameScene = state.GetCurrentGameScene();
        if (gameScene != SCENE_DRIVING && gameScene != SCENE_CRUISING) {
            ((CarScene*)scene)->DrawMaskAfterCar();
        }

        // This changes type every frame, would be good to only change on scene change
        mouse.SetPointerType(scene->GetPointerType());

        mouse.Draw();

        timeDelta = SDL_GetTicks64() - timeDelta;

        state.SetElapsedTime(timeDelta);

        SDL_RenderPresent(renderer);

        // wait 1/60th of a second
        SDL_Delay(1000 / 240);
    }

    //printf("Whadda loser!\n");

    // delete currentScene;
    
    NFD_Quit();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_JoystickClose(controller);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();

}
