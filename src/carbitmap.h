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

    // Convert pointer members to direct objects
    Bitmap bodyOriginal;
    Bitmap bodyChopped;
    Bitmap* body;  // This stays a pointer as it points to either bodyOriginal or bodyChopped
    CarShadow shadow;
    Bitmap driver;
    Spritesheet wheels;  // Single wheels spritesheet
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

public:
    // Default constructor
    CarBitmap();
    
    // Main constructor
    CarBitmap(Car* car, Driver* driver, StandardCarColor color);
    
    // No need for destructor anymore as members clean up automatically
    
    void Draw();
    void Move(SDL_Point p);
    void SetVisible(bool visible);
    void ToggleChoppedRoof();
    void UpdateTires();
    void UpdateTextures();

    // Add missing methods
    void MoveBody(SDL_Point p);
    void MoveWheelsYOffset(int offset);
    int GetWheelsYOffset() const { return wheelsYOffset; }

    // Gas station methods
    void SetGasCapRemoved(bool removed) { 
        gasCapRemoved = removed; 
        gasCap.SetVisible(!removed);
    }
    
    void SetPumpingHandleVisible(bool visible) { 
        pumpingHandleVisible = visible;
        pumpingHandle.SetVisible(visible);
    }
    
    bool IsGasCapRemoved() const { return gasCapRemoved; }
    bool IsPumpingHandleVisible() const { return pumpingHandleVisible; }
};