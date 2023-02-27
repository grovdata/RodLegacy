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
#define ICON "../data/rip/icon.png"

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
Scene *currentScene;

int main(void) {
    // Attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!win) {
        printf("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
	    return 1;
    }

    SDL_Surface* icon = IMG_Load(ICON);
	if (icon == NULL) {
		printf("Could not load icon");
	} else {
		SDL_SetWindowIcon(win, icon);
	}

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);


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

    currentScene = new Garage();

    // struct to hold the position and size of the sprite
    SDL_Rect dest;

    // get and scale the dimensions of texture
    SDL_QueryTexture(currentScene->GetTexture(), NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    bool close_requested = false;

    while (!close_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            currentScene->PushEvent(&event);

            switch (event.type) {
                case SDL_QUIT:
                    close_requested = true;
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
        currentScene->Draw();
        mouse.Draw();
        SDL_RenderPresent(renderer);

        // wait 1/60th of a second
        SDL_Delay(1000/60);
    }

    printf("Whadda loser!\n");

    delete currentScene;

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
}
