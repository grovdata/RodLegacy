#pragma once
#include "global.h"
#include "car.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

class CarShadow: public Bitmap {
    private:
        int width;
    public:
        CarShadow(int width);
        ~CarShadow();
        void Draw() override;
};

class CarBitmap {
   private:
    Car *car;

    Bitmap *bodyOriginal;  // Original body without chopped roof
    Bitmap *bodyChopped;   // Body with chopped roof
    Bitmap *body;          // Pointer to current active body (original or chopped)
    CarShadow *shadow;
    bool isRoofChopped;

    Bitmap *driver;
    Spritesheet *rearWheel;
    Spritesheet *frontWheel;
    Bitmap *gasCap;
    Bitmap *pumpingHandle;
    Bitmap *hoodScoop;
    Spritesheet *decal;

    Uint64 t;

    BitmapOffset_t offsets;
    int wheelsYOffset;
    bool showDriver;

    Uint64 lastWheelUpdateTime = 0;  // Track last wheel animation update time

    bool wheelsVisible = true;  // Add this member variable

    StandardCarColor currentColor;

    void UpdateTextures();
    void UpdateTires();
    void OverlayPart(Bitmap* baseBitmap, Bitmap* partBitmap, int xOffset, int yOffset);

   public:
    CarBitmap(Car *car, Driver* driver, StandardCarColor color);
    ~CarBitmap();

    void Draw();
    void Move(SDL_Point p);
    void MoveBody(SDL_Point p);
    void MoveWheelsYOffset(int y);
    void UpdateWheelAnimation(float speed);

    BitmapOffset_t GetOffset();

    int GetWheelsYOffset() const { return wheelsYOffset; }
    bool AreWheelsVisible() const { return wheelsVisible; }
    void ToggleWheels() { wheelsVisible = !wheelsVisible; }

    void UpdateColor(StandardCarColor newColor);

    void ToggleChoppedRoof();
    void SetGasCapRemoved(bool removed);
    void SetPumpingHandleVisible(bool visible);
    bool IsRoofChopped() const { return isRoofChopped; }
};