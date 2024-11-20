#pragma once
#include "global.h"
#include "car.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"

class CarBitmap {
   private:
    Car *car;


    Bitmap *body;
    Bitmap *driver;
    Spritesheet *rearWheel;
    Spritesheet *frontWheel;
    Bitmap *gasCap;
    Bitmap *hoodScoop;
    Spritesheet *decal;

    Uint64 t;

    BitmapOffset_t offsets;

    bool showDriver;

    void UpdateTextures();

   public:
    CarBitmap(Car *car, Driver* driver);
    ~CarBitmap();

    void Draw();
    void Move(SDL_Point p);
};