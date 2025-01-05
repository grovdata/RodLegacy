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
    Car* car;  // This stays a pointer as it's managed elsewhere
    Bitmap bodyOriginal;
    Bitmap bodyChopped;
    Bitmap* body;  // Points to either bodyOriginal or bodyChopped
    CarShadow shadow;
    Bitmap driver;
    Spritesheet wheels;
    Bitmap gasCap;
    Bitmap pumpingHandle;
    Bitmap hoodScoop;
    Bitmap rearWheelhouse;
    Bitmap frontWheelhouse;
    Spritesheet decal;
    Bitmap rearWheelhouseBackground;
    Bitmap frontWheelhouseBackground;

    SDL_Rect frontWheelPos;
    SDL_Rect rearWheelPos;
    
    Uint64 t;
    BitmapOffset_t offsets;
    int wheelsYOffset;
    bool showDriver;
    bool isRoofChopped;
    Uint64 lastWheelUpdateTime;
    bool wheelsVisible;
    StandardCarColor currentColor;
    bool gasCapRemoved;
    bool pumpingHandleVisible;

    void OverlayPart(Bitmap* baseBitmap, Bitmap* partBitmap, int xOffset, int yOffset);

public:
   
    // Main constructor
    CarBitmap(Car* car, Driver* driver, StandardCarColor color);
    
    // No need for destructor anymore as members clean up automatically
    void LoadCarParts(Car *car, Driver *driver, StandardCarColor color);

    void Draw();
    void Move(SDL_Point p);
    void ToggleChoppedRoof();
    void UpdatePositions();
    void UpdateTires();
    void UpdateWheelAnimation(float speed);
    void UpdateTextures();
    void UpdateColor(StandardCarColor newColor);

    // Add missing methods
    void MoveBody(SDL_Point p);
    void MoveWheelsYOffset(int offset);
    int GetWheelsYOffset() const { return wheelsYOffset; }
    BitmapOffset_t GetOffset();

    // Gas station methods
    void SetGasCapRemoved(bool removed);
    void SetPumpingHandleVisible(bool visible);
    
    bool IsGasCapRemoved() const { return gasCapRemoved; }
    bool IsPumpingHandleVisible() const { return pumpingHandleVisible; }
};