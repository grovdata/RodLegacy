#pragma once
#include "car.h"
#include "global.h"

class CarBitmap {
   private:
    Car *car;

    // Body
    SDL_Surface *bodySurface;
    SDL_Texture *bodyTexture;
    SDL_Rect bodyRect;

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
    SDL_Point gasCapLocation;

    // Hood scoop

    Uint64 t;

    void UpdateTextures();

   public:
    CarBitmap(Car *car);
    ~CarBitmap();
    SDL_Texture *GetTexture();
    void Draw();
    void Move(SDL_Point p);
    void MoveLeft();
    void MoveRight();
};