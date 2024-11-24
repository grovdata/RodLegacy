#include "global.h"
#include "mouse.h"
#include "game.h"
#include "parsers/libp.h"
#include "state.h"
#include "scene.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "debug_overlay.h"

#define BITPLANE_DEPTH 4

#define WINDOW_WIDTH (GAME_WIDTH * 4)
#define WINDOW_HEIGHT (GAME_HEIGHT * 4)

#define FONT "../data/font/font.png"
#define DEBUG_FONT "../data/font/lemon.png"
#define TUNE "../data/snd/sr1_ost.ogg"

#define ICON "../data/res/icon.png"
#define APP_TITLE "Street Rod Legacy"

// speed in pixels/second
#define SPEED (300)

SDL_Window *win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SFont_Font *font = SFont_InitFont(IMG_Load(FONT));
SFont_Font *debugFont = SFont_InitFont(IMG_Load(DEBUG_FONT));
SDL_Joystick *controller = NULL;
Uint64 startTime = SDL_GetTicks64();

Game game;
Mouse mouse;
BitmapParser b;
bool isFullScreen = false;

float scaleX = 1.0f;
float scaleY = 1.0f;

DebugOverlay debugOverlay(renderer, debugFont);

void ToggleFullScreen(SDL_Window* window, bool currentState) {
    isFullScreen = !currentState;
    int r = SDL_SetWindowFullscreen(window, isFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(currentState);
}

int main(int argc, char *argv[]) {

    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        SDL_Log("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!win) {
        printf("error creating window: %s\n", SDL_GetError());
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

    // Res scaling
    SDL_RenderSetLogicalSize(renderer, GAME_WIDTH, GAME_HEIGHT);

    int windowWidth, windowHeight;
    int logicalWidth, logicalHeight;
    SDL_GetWindowSize(win, &windowWidth, &windowHeight);
    SDL_RenderGetLogicalSize(renderer, &logicalWidth, &logicalHeight);

    scaleX = (float)windowWidth / logicalWidth;
    scaleY = (float)windowHeight / logicalHeight;

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

    bool debugMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        }
    }

    if (debugMode) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForSDLRenderer(win, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // Adjust style for tiny UI
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(2, 2);
        style.FramePadding = ImVec2(1, 1);
        style.ItemSpacing = ImVec2(2, 1);
        style.ItemInnerSpacing = ImVec2(1, 1);
        style.TouchExtraPadding = ImVec2(0, 0);
        style.ScrollbarSize = 8;
        
        // Scale everything down even more
        style.ScaleAllSizes(0.5f);

        // Make font tiny
        ImGuiIO& io = ImGui::GetIO();
        //io.Fonts->AddFontFromFileTTF("include/imgui/misc/fonts/ProggyTiny.ttf", 10.0f);
        io.FontGlobalScale = 0.5f;
    }

    while (!close_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (debugMode) {
                // Create a copy of the event for ImGui
                SDL_Event imguiEvent = event;
                
                // Scale mouse coordinates UP for ImGui copy
                if (imguiEvent.type == SDL_MOUSEMOTION) {
                    imguiEvent.motion.x = (int)(imguiEvent.motion.x * scaleX);
                    imguiEvent.motion.y = (int)(imguiEvent.motion.y * scaleY);
                }
                else if (imguiEvent.type == SDL_MOUSEBUTTONDOWN || imguiEvent.type == SDL_MOUSEBUTTONUP) {
                    imguiEvent.button.x = (int)(imguiEvent.button.x * scaleX);
                    imguiEvent.button.y = (int)(imguiEvent.button.y * scaleY);
                }
                
                std::cout << "imgui mouse: " << imguiEvent.motion.x << ", " << imguiEvent.motion.y << std::endl;
                
                ImGui_ImplSDL2_ProcessEvent(&imguiEvent);
                
                ImGuiIO& io = ImGui::GetIO();
                if (io.WantCaptureMouse && (
                    event.type == SDL_MOUSEMOTION ||
                    event.type == SDL_MOUSEBUTTONDOWN ||
                    event.type == SDL_MOUSEBUTTONUP ||
                    event.type == SDL_MOUSEWHEEL))
                {
                    continue;
                }
            }
            
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
        GameScene gameScene = state.GetCurrentGameScene();
        if (gameScene != SCENE_DRIVING && gameScene != SCENE_CRUISING) {
            ((CarScene*)scene)->DrawMaskAfterCar();
        }

        mouse.Draw();

        timeDelta = SDL_GetTicks64() - timeDelta;

        game.Update(timeDelta);
        state.SetElapsedTime(timeDelta);

        if (debugMode) {
            debugOverlay.Clear();
            //debugOverlay.AddLine("FPS: " + std::to_string(1000.0f / timeDelta));
            debugOverlay.AddLine("Scene: " + std::to_string(state.GetCurrentGameScene()));
            // debugOverlay.AddLine("Mouse: " + std::to_string(mouse.x) + ", " + std::to_string(mouse.y));
            debugOverlay.Draw();
        }

        SDL_RenderPresent(renderer);

        // wait 1/60th of a second
        SDL_Delay(1000 / 240);
    }

    //printf("Whadda loser!\n");

    // delete currentScene;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_JoystickClose(controller);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();

    if (debugMode) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
}
