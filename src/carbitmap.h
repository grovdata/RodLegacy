#pragma once
#include "global.h"
#include "car.h"
#include "bitmap.h"
#include "spritesheet.h"

class CarBitmap {
   private:
    Car *car;

    // Body
    Bitmap body;

    // Driver
    SDL_Surface *driverSurface;
    SDL_Texture *driverTexture;
    SDL_Rect driverRect;

    // Wheels
    Spritesheet *rearWheel;
    Spritesheet *frontWheel;

    SDL_Rect rearWheelRect;
    SDL_Rect frontWheelRect;

    // Gas cap
    Bitmap gasCap;
    Bitmap hoodScoop;

    Uint64 t;

    void UpdateTextures();

   public:
    CarBitmap(Car *car);
    ~CarBitmap();
//    SDL_Texture *GetTexture();
    void Draw();
    void Move(SDL_Point p);
};