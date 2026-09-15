#pragma once
#include "core/global.h"
#include "car/car.h"
#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

// Loads one of a car's sprites (body, chopped roof, bumper patch) from
// whichever lib the model draws from. Pair it with CarBodySprite and friends
// in types.h; a negative index gives nullptr. Amiga lib.p art is recoloured
// through carPalette, while Street Rod SE cars are fixed 16-colour DOS
// sprites that already carry their paint, so carPalette is ignored for those.
// Driver portrait Y relative to the top-left of the car body. One value for
// every car, even though SpriteInfo carries a per-car y_driver that nothing
// reads - see CarBitmap::UpdateTextures.
extern const int kDriverBaseY;

Bitmap* LoadCarModelSprite(const CarModel& model, int spriteIndex,
                           const std::vector<uint16_t>& carPalette);

// One wheel rotation cell, for callers that want a still image rather than an
// animated CarWheel (the newspaper preview). outSourceRect receives the
// sub-rect to blit - a cell of the lib.p spritesheet, or a zero rect for a DOS
// car set, where the whole sprite is the frame. outSize receives the size to
// draw at, which differs between sets.
Bitmap* GetCarWheelFrame(const CarModel& model, CarWheelType type, int frame,
                         SDL_Rect* outSourceRect, SDL_Point* outSize);

// A car's wheel: the rotation cells, however its set happens to store them.
// lib.p packs all four into one 1x4 spritesheet; a DOS car set gives each cell
// its own sprite id. Both are just N frames you pick between and draw, and
// this hides that difference and nothing else - the frames keep their own
// natural size, which differs between the two (32x31 vs 40x28).
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
    // Shared so a cached CarBitmap can never outlive its car (loading a save
    // replaces State's whole roster).
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
    Uint64 lastWheelUpdateTime;
    Uint64 lastDrawWheelTime = 0;
    int rollingFrame = 0;
    bool wheelsVisible;
    StandardCarColor currentColor;
    bool gasCapRemoved;

    bool stickerWipeActive = false;
    float stickerWipeProgress = 0.0f;
    Bitmap stickerRoll;

    // Reloads body/bodyChopped from assets with the current palette, honoring
    // isRoofChopped. Shared by RestoreFrontBumper/RestoreRearBumper.
    void ReloadBodyFromAssets();

public:
   
    // Main constructor
    CarBitmap(CarPtr car, Driver* driverInfo, StandardCarColor color);
    
    // No need for destructor anymore as members clean up automatically
    void LoadCarParts(CarPtr car, Driver *driver, StandardCarColor color);

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

    // Live nudge on the driver portrait, on top of the model's own offsets.
    // See CarDebug; the driver's Y has no per-car source today.
    void MoveDriverOffset(int dx, int dy);
    SDL_Point GetDriverOffset() const { return driverOffset; }
    // Where the driver ends up relative to the top-left of the car body, which
    // is the frame SpriteInfo's x_driver / y_driver are expressed in.
    SDL_Point GetDriverOffsetFromBody() const;
    StandardCarColor GetCurrentColor() const { return currentColor; }
    Car* GetCar() const { return car.get(); }
    const CarPtr& GetCarPtr() const { return car; }
    BitmapOffset_t GetOffset();

    // Tire change animation methods
    SDL_Rect GetFrontWheelPos() const { return frontWheelPos; }
    SDL_Rect GetRearWheelPos() const { return rearWheelPos; }
    void SetWheelsVisible(bool visible) { wheelsVisible = visible; }
    // For the win cutscene: once she has climbed out, the portrait behind the
    // windscreen has to go.
    void SetDriverVisible(bool visible) { driver.SetVisible(visible); }
    void ResetWheelFrame() { wheels.SelectFrame(0); }
    void SetWheelhousesVisible(bool visible);
    void DrawWheelAt(int x, int y, bool spin = true, bool reverse = false);
    void SetShadowYOffset(int yAdjust);
    void LoadNewTireSprite(CarWheelType type);

    // Gas station methods
    void SetGasCapRemoved(bool removed);
    void SetShadowVisible(bool visible);

    bool IsGasCapRemoved() const { return gasCapRemoved; }
    
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