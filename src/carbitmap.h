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

    Uint64 lastWheelUpdateTime = 0;  // Track last wheel animation update time

    void UpdateTextures();

   public:
    CarBitmap(Car *car, Driver* driver);
    ~CarBitmap();

    void Draw();
    void Move(SDL_Point p);
    void UpdateWheelAnimation(float speed);

    BitmapOffset_t GetOffset();
};