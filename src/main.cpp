#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "driver.hpp"

#define WINDOW_WIDTH (640)
#define WINDOW_HEIGHT (400)

#define TUNE "../data/snd/sr1_ost.ogg"

// speed in pixels/second
#define SPEED (300)

int main(void)
{
    // attempt to initialize graphics and timer system
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) {
        printf("error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* win = SDL_CreateWindow("OpenRod",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       WINDOW_WIDTH, WINDOW_HEIGHT,0);
    if (!win) {
        printf("error creating window: %s\n", SDL_GetError());
        SDL_Quit();
	    return 1;
    }

    // Initialize SDL2_mixer
    if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        printf("SDL2_mixer could not be initialized!\n"
               "SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    Mix_Music *music = Mix_LoadMUS(TUNE);
    Mix_PlayMusic(music, 1);

    // create a renderer, which sets up the graphics hardware
    Uint32 render_flags = SDL_RENDERER_ACCELERATED;
    SDL_Renderer* rend = SDL_CreateRenderer(win, -1, render_flags);
    if (!rend) {
      printf("error creating renderer: %s\n", SDL_GetError());
      SDL_DestroyWindow(win);
      SDL_Quit();
      return 1;
    }

    // res scaling
    SDL_RenderSetLogicalSize(rend, 320, 200);

    // Force integer scaling
    //SDL_RenderSetIntegerScale(rend, SDL_TRUE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // load the image into memory using SDL_image library function
    SDL_Surface* surface = IMG_Load("../data/rip/title.png");

    if (!surface) {
        printf("error creating surface\n");
        SDL_DestroyRenderer(rend);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // load the image data into the graphics hardware's memory
    SDL_Texture* tex = SDL_CreateTextureFromSurface(rend, surface);
    SDL_FreeSurface(surface);
    if (!tex) {
        printf("error creating texture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(rend);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // struct to hold the position and size of the sprite
    SDL_Rect dest;

    // get and scale the dimensions of texture
    SDL_QueryTexture(tex, NULL, NULL, &dest.w, &dest.h);
    dest.x = 0;
    dest.y = 0;

    // set to 1 when window close button is pressed
    int close_requested = 0;

    // animation loop
    while (!close_requested) {
        // process events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                close_requested = 1;
            }
        }

        // clear the window
        SDL_RenderClear(rend);

        // draw the image to the window
        SDL_RenderCopy(rend, tex, NULL, &dest);
        SDL_RenderPresent(rend);

        // wait 1/60th of a second
        SDL_Delay(1000/60);
    }

    printf("Whadda loser!\n");


    // clean up resources before exiting
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(rend);
    SDL_DestroyWindow(win);
    Mix_FreeMusic(music);
    Mix_CloseAudio();
    SDL_Quit();
}
