#pragma once
#include "core/global.h"
#include "car/car.h"
#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

// A negative index gives nullptr. SE cars are fixed-colour DOS sprites, so carPalette is ignored for them.
Bitmap* LoadCarModelSprite(const CarModel& model, int spriteIndex,
                           const std::vector<uint16_t>& carPalette);

// A still wheel cell. outSourceRect is a lib.p sheet cell, or zero for a DOS set; outSize differs per set.
Bitmap* GetCarWheelFrame(const CarModel& model, CarWheelType type, int frame,
                         SDL_Rect* outSourceRect, SDL_Point* outSize);

// lib.p packs the cells into one sheet (32x31), a DOS set gives each its own sprite (40x28).
class CarWheel {
public:
    void SetSpritesheet(const AnimatedBitmap& sheet, int drawW, int drawH);
    void SetFrames(std::vector<Bitmap> cells);

    bool IsLoaded() const { return frameCount > 0; }
    int  FrameCount() const { return frameCount; }
    int  Width() const { return frameW; }
    int  Height() const { return frameH; }

    void SelectFrame(int index);
    void CycleNextFrame();
    void CyclePrevFrame();
    int  CurrentFrame() const { return current; }

    void DrawAt(int x, int y);

private:
    AnimatedBitmap sheet;       // lib.p backing
    std::vector<Bitmap> cells;  // DOS car set backing
    bool sheetBacked = false;
    int frameCount = 0;
    int current = 0;
    int frameW = 0;
    int frameH = 0;
};

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
    // Shared so a cached CarBitmap never outlives its car when a save replaces the roster.
    CarPtr car;
    Bitmap bodyOriginal;
    Bitmap bodyChopped;
    Bitmap* body;  // Points to either bodyOriginal or bodyChopped
    CarShadow shadow;
    Bitmap driver;
    CarWheel wheels;
    Bitmap gasCap;
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
    SDL_Point driverOffset = {0, 0};
    bool isRoofChopped;
    bool isFrontBumperStripped;
    bool isRearBumperStripped;
    Uint64 lastDrawWheelTime = 0;
    int rollingFrame = 0;
    bool wheelsVisible;
    StandardCarColor currentColor;
    bool gasCapRemoved;

    bool stickerWipeActive = false;
    float stickerWipeProgress = 0.0f;
    Bitmap stickerRoll;

    void ReloadBodyFromAssets();

public:
   
    CarBitmap(CarPtr car, Driver* driverInfo, StandardCarColor color);
    
    void LoadCarParts(CarPtr car, Driver *driver, StandardCarColor color);

    void Draw();
    void Move(SDL_Point p);
    void ToggleChoppedRoof();
    void ChopRoof();
    void RestoreRoof();
    void UpdatePositions();
    void UpdateWheelRotation(int carX);
    void UpdateTextures();  
    void UpdateColor(StandardCarColor newColor);

    void MoveBody(SDL_Point p);
    void MoveWheelsYOffset(int offset);
    int GetWheelsYOffset() const { return wheelsYOffset; }

    void MoveDriverOffset(int dx, int dy);
    SDL_Point GetDriverOffset() const { return driverOffset; }
    // The frame SpriteInfo's x_driver / y_driver are expressed in.
    SDL_Point GetDriverOffsetFromBody();
    StandardCarColor GetCurrentColor() const { return currentColor; }
    Car* GetCar() const { return car.get(); }
    const CarPtr& GetCarPtr() const { return car; }
    BitmapOffset_t GetOffset();

    SDL_Rect GetFrontWheelPos() const { return frontWheelPos; }
    SDL_Rect GetRearWheelPos() const { return rearWheelPos; }
    void SetWheelsVisible(bool visible) { wheelsVisible = visible; }
    void SetDriverVisible(bool visible) { driver.SetVisible(visible); }
    void ResetWheelFrame() { wheels.SelectFrame(0); }
    void SetWheelhousesVisible(bool visible);
    void DrawWheelAt(int x, int y, bool spin = true, bool reverse = false);
    void SetShadowYOffset(int yAdjust);
    void LoadNewTireSprite(CarWheelType type);

    void SetGasCapRemoved(bool removed);
    void SetShadowVisible(bool visible);

    bool IsGasCapRemoved() const { return gasCapRemoved; }
    
    void StripFrontBumper();
    void StripRearBumper();
    void RestoreFrontBumper();
    void RestoreRearBumper();
    bool IsFrontBumperStripped() const { return isFrontBumperStripped; }
    bool IsRearBumperStripped() const { return isRearBumperStripped; }
    
    bool IsRoofChopped() const { return isRoofChopped; }

    void StartStickerWipe();
    void UpdateStickerWipe(float deltaTime);
    bool IsStickerWipeActive() const { return stickerWipeActive; }

    static SDL_Surface* CreateColorSwatch(StandardCarColor color, int width = 10, int height = 6);
};