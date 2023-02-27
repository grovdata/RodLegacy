#include "global.h"
#include "driver.h"
#include "garage.h"
#include "mouse.h"
#include "ui/button.h"
#include "ui/zone.h"

#define WINDOW_WIDTH (640)
#define WINDOW_HEIGHT (400)

#define FONT "../data/font/font.png"
#define TUNE "../data/snd/sr1_ost.ogg"
#define BACKGROUND "../data/rip/garage.png"

// speed in pixels/second
#define SPEED (300)

SDL_Window* win = SDL_CreateWindow("OpenRod",
                                    SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    WINDOW_WIDTH, WINDOW_HEIGHT, 0);

SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

SFont_Font *font = SFont_InitFont(IMG_Load(FONT));

Mouse mouse;
Button *testButton, *testButton2;
Garage *garage;

int main(void) {
    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (!win) {
        printf("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
	    return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    // Font


    //SFont_DumpFont(Font);
    testButton = new Button("Pink slips", 100, 100, 82, cButtonStandardHeight);
    testButton2 = new Button("Drag race", 100, 100+cButtonStandardHeight, 82, cButtonStandardHeight);

    // Initialize SDL2_mixer
    if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        printf("SDL2_mixer could not be initialized!\n"
               "SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    Mix_Music *music = Mix_LoadMUS(TUNE);
    Mix_PlayMusic(music, 1);

    if (!renderer) {
      printf("error creating renderer: %s\n", SDL_GetError());
      SDL_DestroyWindow(win);
      SDL_Quit();
      return 1;
    }

    // Res scaling
    SDL_RenderSetLogicalSize(renderer, 320, 200);

    // Force integer scaling
    //SDL_RenderSetIntegerScale(rend, SDL_TRUE);

    garage = new Garage();

    // // Load the image into memory using SDL_image library function
    // SDL_Surface* surface = IMG_Load(BACKGROUND);

    // if (!surface) {
    //     printf("error creating surface\n");
    //     SDL_DestroyRenderer(renderer);
    //     SDL_DestroyWindow(win);
    //     SDL_Quit();
    //     return 1;
    // }


    // // load the image data into the graphics hardware's memory
    // SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    // SDL_FreeSurface(surface);
    // if (!tex) {
    //     printf("error creating texture: %s\n", SDL_GetError());
    //     SDL_DestroyRenderer(renderer);
    //     SDL_DestroyWindow(win);
    //     SDL_Quit();
    //     return 1;
    // }

    // struct to hold the position and size of the sprite
    SDL_Rect dest;

    // get and scale the dimensions of texture
    SDL_QueryTexture(garage->GetTexture(), NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    // set to 1 when window close button is pressed
    int close_requested = 0;

    // animation loop
    while (!close_requested) {


        // process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            garage->PushEvent(&event);

            switch (event.type) {
                case SDL_QUIT:
                    close_requested = 1;
                    break;
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONUP:
                    mouse.Update(&event);
                    break;
                default:
                    break;
            }

        }

        SDL_RenderClear(renderer);
        garage->Draw();
        mouse.Draw();
        SDL_RenderPresent(renderer);

        // wait 1/60th of a second
        SDL_Delay(1000/60);
    }

    printf("Whadda loser!\n");

    delete garage;

    // clean up resources before exiting
    // SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
}
