#include "diner.h"
#include "driver.h"
#include "game.h"
#include "garage.h"
#include "global.h"
#include "mouse.h"
#include "ui/button.h"
#include "ui/zone.h"
#include "state.h"

#define WIDTH 32
#define HEIGHT 20
#define BITPLANE_DEPTH 4

// #define WINDOW_WIDTH (640)
// #define WINDOW_HEIGHT (400)
#define WINDOW_WIDTH (1280)
#define WINDOW_HEIGHT (800)

#define FONT "../data/font/font.png"
#define TUNE "../data/snd/sr1_ost.ogg"

#define ICON "../data/res/icon.png"
#define APP_TITLE "Street Rod Legacy"

// speed in pixels/second
#define SPEED (300)

SDL_Window *win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SFont_Font *font = SFont_InitFont(IMG_Load(FONT));
SDL_Joystick *controller = NULL;
Uint64 startTime = SDL_GetTicks64();

Game game;
Mouse mouse;

// Scenes
Garage garage;
Diner diner;


// TODO: Move this somewhere else
BitmapParser b;
int main(void) {
    // TODO: Move this somewhere else
    // vector<LibBitmapEntry_t> result =
    // b.Parse(strdup("../amiga/SR_disk2/lib.p")); LibBitmapEntry_t first =
    // result.at(0); uint8_t databuf[first.sizePacked];
    // b.GetData(strdup("../amiga/SR_disk2/lib.p"), databuf, &first,
    // result.size());
    // SDL_Surface *bmpsurface = SDL_CreateRGBSurface(0, first.width, first.width,
    // 8, 0, 0, 0, 0); b.Unpack(&first, databuf, bmpsurface);
    // b.Parse(strdup("../amiga/SR_disk2/lib.p"));

    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
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
    SDL_RenderSetLogicalSize(renderer, 320, 200);

    // Force integer scaling
    // SDL_RenderSetIntegerScale(rend, SDL_TRUE);

    // currentScene = new Garage(&game);

    state.SetCurrentScene(&diner);


    // temp
    // CarModel model;
    // model.vendor = GM;
    // model.name = "Chevy";
    // model.filename = "../data/res/car1.gif";
    // model.year = 1964;
    // model.baseSpeed = 98;
    // model.width = 232;
    // model.height = 69;

    CarModel model;
    model.vendor = GM;
    model.name = "Saab 92";
    model.filename = "../data/mods/saab92.png";
    model.year = 1948;
    model.baseSpeed = 98;
    model.width = 140;
    model.height = 51;
    model.rearWheelX = 15;
    model.rearWheelY = 23;
    model.frontWheelX = 102;
    model.frontWheelY = 23;
    model.driverX = 66;
    model.driverY = 2;


    Car car(model);
    car.Move(state.GetCurrentScene()->GetCarIdlePosition());

    // struct to hold the position and size of the sprite
    SDL_Rect dest;

    // get and scale the dimensions of texture
    SDL_QueryTexture(state.GetCurrentScene()->GetTexture(), NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    bool close_requested = false;

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
                            car.MoveLeft();
                            break;
                        case SDLK_RIGHT:
                            car.MoveRight();
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
                        case SDLK_g:
                            state.SetCurrentScene(&garage);
                            car.Move(state.GetCurrentScene()->GetCarIdlePosition());
                            break;
                        case SDLK_d:
                            state.SetCurrentScene(&diner);
                            car.Move(state.GetCurrentScene()->GetCarOffscreenPosition());
                            diner.StartAnimateCar(&car, nullptr, false);
                            break;
                        case SDLK_p:
                            mouse.SetPointerType(PointerFuelPump);
                            break;
                        case SDLK_ESCAPE:
                            close_requested = true;
                            break;
                        case SDLK_LEFT:
                            car.MoveLeft();
                            break;
                        case SDLK_RIGHT:
                            car.MoveRight();
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
        state.GetCurrentScene()->Draw();
        car.Move(diner.CarPos());
        car.Draw();
        state.GetCurrentScene()->DrawMaskAfterCar();

        mouse.Draw();
        game.Update(SDL_GetTicks64() - startTime);
        state.SetElapsedTime(SDL_GetTicks64() - startTime);

        // #include "planetest.c"
        // SDL_Surface* convertedSurface;
        // planetest(convertedSurface);

        // SDL_PixelFormat f = SDL_GetWindowPixelFormat();
        //  SDL_Surface* convertedSurface = SDL_ConvertSurface(bmpsurface, &f, 0);

        // SDL_Texture *temp = SDL_CreateTextureFromSurface(renderer,
        // convertedSurface); SDL_Rect r = {0,0,first.width, first.height};
        // SDL_RenderCopy(renderer, temp, NULL, &r);
        SDL_RenderPresent(renderer);

        // wait 1/60th of a second
        SDL_Delay(1000 / 240);
    }

    printf("Whadda loser!\n");

    // delete currentScene;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_JoystickClose(controller);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
}
