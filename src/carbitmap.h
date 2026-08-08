#pragma once
#include "global.h"
#include "car.h"
#include "bitmap.h"
#include "animated_bitmap.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

// Loads one of a car's sprites (body, chopped roof, bumper patch) from
// whichever lib the model draws from. Pair it with CarBodySprite and friends
// in types.h; a negative index gives nullptr. Amiga lib.p art is recoloured
// through carPalette, while Street Rod SE cars are fixed 16-colour DOS
// sprites that already carry their paint, so carPalette is ignored for those.
Bitmap* LoadCarModelSprite(const CarModel& model, int spriteIndex,
                           const std::vector<uint16_t>& carPalette);

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
    AnimatedBitmap wheels;
    Bitmap gasCap;
    Bitmap pumpingHandle;
    Bitmap hoodScoop;
    Bitmap rearWheelhouse;
    Bitmap frontWheelhouse;
    AnimatedBitmap decal;
    Bitmap customDecal;
    Bitmap rearWheelhouseBackground;
    Bitmap frontWheelhouseBackground;
    Bitmap rearBumper;
    Bitmap frontBumper;
    
    SDL_Rect frontWheelPos;
    SDL_Rect rearWheelPos;
    
    Uint64 t;
    BitmapOffset_t offsets;
    int wheelsYOffset;
    bool isRoofChopped;
    bool isFrontBumperStripped;
    bool isRearBumperStripped;
    Uint64 lastWheelUpdateTime;
    Uint64 lastDrawWheelTime = 0;
    int rollingFrame = 0;
    bool wheelsVisible;
    StandardCarColor currentColor;
    bool gasCapRemoved;
    bool pumpingHandleVisible;

    bool stickerWipeActive = false;
    float stickerWipeProgress = 0.0f;
    Bitmap stickerRoll;

    // Reloads body/bodyChopped from assets with the current palette, honoring
    // isRoofChopped. Shared by RestoreFrontBumper/RestoreRearBumper.
    void ReloadBodyFromAssets();

public:
   
    // Main constructor
    CarBitmap(Car* car, Driver* driverInfo, StandardCarColor color);
    
    // No need for destructor anymore as members clean up automatically
    void LoadCarParts(Car *car, Driver *driver, StandardCarColor color);

    void Draw();
    void Move(SDL_Point p);
    void ToggleChoppedRoof();
    void ChopRoof();
    void RestoreRoof();
    void UpdatePositions();
    void UpdateTires();
    void UpdateWheelAnimation(float speed);
    void UpdateTextures();  
    void UpdateColor(StandardCarColor newColor);

    // Add missing methods
    void MoveBody(SDL_Point p);
    void MoveWheelsYOffset(int offset);
    int GetWheelsYOffset() const { return wheelsYOffset; }
    StandardCarColor GetCurrentColor() const { return currentColor; }
    Car* GetCar() const { return car; }
    BitmapOffset_t GetOffset();

    // Tire change animation methods
    SDL_Rect GetFrontWheelPos() const { return frontWheelPos; }
    SDL_Rect GetRearWheelPos() const { return rearWheelPos; }
    void SetWheelsVisible(bool visible) { wheelsVisible = visible; }
    void ResetWheelFrame() { wheels.SelectFrame(0, 0); }
    void SetWheelhousesVisible(bool visible);
    void DrawWheelAt(int x, int y, bool spin = true, bool reverse = false);
    void SetShadowYOffset(int yAdjust);
    void LoadNewTireSprite(CarWheelType type);

    // Gas station methods
    void SetGasCapRemoved(bool removed);
    void SetPumpingHandleVisible(bool visible);
    void SetShadowVisible(bool visible);
    
    bool IsGasCapRemoved() const { return gasCapRemoved; }
    bool IsPumpingHandleVisible() const { return pumpingHandleVisible; }
    
    // Bumper methods
    void StripFrontBumper();
    void StripRearBumper();
    void RestoreFrontBumper();
    void RestoreRearBumper();
    bool IsFrontBumperStripped() const { return isFrontBumperStripped; }
    bool IsRearBumperStripped() const { return isRearBumperStripped; }
    
    // Roof methods
    bool IsRoofChopped() const { return isRoofChopped; }

    void StartStickerWipe();
    void UpdateStickerWipe(float deltaTime);
    bool IsStickerWipeActive() const { return stickerWipeActive; }

    static SDL_Surface* CreateColorSwatch(StandardCarColor color, int width = 10, int height = 6);
};