#pragma once

#include "global.h"
#include "scene.h"
#include "mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "dialogs/list_dialog.h"
#include "carbitmap.h"
#include "engine_bay.h"
#include "animated_bitmap.h"
#include "parsers/carcolor.h"
#include <functional>
#include <filesystem>

class Garage : public CarScene {
   private:
    enum class TireChangePhase {
        IDLE,
        JACK_SLIDE_UP,
        JACK_CRANK_UP,
        REAR_WHEEL_OUT,
        FRONT_WHEEL_OUT,
        NEW_REAR_WHEEL_IN,
        NEW_FRONT_WHEEL_IN,
        JACK_CRANK_DOWN,
        JACK_SLIDE_DOWN,
    };

    TireChangePhase tireChangePhase = TireChangePhase::IDLE;
    float tireChangeTimer = 0.0f;
    CarWheelType pendingTireType = STOCK;
    std::unique_ptr<ListDialog> tireDialog;
    std::unique_ptr<AnimatedBitmap> jackAnim;
    SDL_Rect initialFrontWheelPos{};
    SDL_Rect initialRearWheelPos{};
    float animFrontWheelX = 0.0f;
    float animRearWheelX = 0.0f;
    float animFrontWheelY = 0.0f;
    float animRearWheelY = 0.0f;
    float jackYPos = 0.0f;
    float jackStartY = 0.0f;
    int tireChangeLiftY = 0;
    int jackTotalFrames = 0;
    int lastJackCrankSounded = -1;
    SDL_Rect jackLastFrameRect{};
    static constexpr int JACK_CRANKS = 7;

    void ShowTireDialog();
    void StartTireChangeAnimation(CarWheelType newType);
    void UpdateTireChangeAnimation(double deltaTime);

    Car *pendingCar = nullptr;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    std::unique_ptr<ListDialog> carSelectDialog;
    std::unique_ptr<ListDialog> loadGameDialog;

    // A save the "Load Game" dialog can offer: either a real file in the
    // game directory, or a sample save embedded in disk2's ADF image (the
    // original Amiga disk ships a handful in its root directory).
    struct LoadGameEntry {
        std::string label;
        bool fromDisk2 = false;
        std::filesystem::path gameDirPath;  // valid when !fromDisk2
        std::string adfFileName;            // valid when fromDisk2
    };
    std::vector<LoadGameEntry> loadGameFiles;
    // Scanned once from Init() (see ScanDisk2Samples's own comment for why
    // not lazily on first dialog open).
    std::vector<LoadGameEntry> cachedDisk2Samples;
    void ScanDisk2Samples();
    void ShowLoadGameDialog();
    bool LoadSelectedGame(const LoadGameEntry& entry);
    std::unique_ptr<Dialog> gameMenuDialog;
    std::unique_ptr<Dialog> optionsDialog;
    std::unique_ptr<Dialog> frontBumperDialog;
    std::unique_ptr<Dialog> rearBumperDialog;
    std::unique_ptr<Dialog> roofDialog;
    std::unique_ptr<Dialog> paintDialog;
    std::unique_ptr<EngineBay> engineBay;
    bool fullscreenEnabled   = false;
    bool rebuildOptions_     = false;
    StandardCarColor previewColor;
    StandardCarColor originalColor;
    std::unique_ptr<Bitmap> paintCanPreview;
    std::unique_ptr<Dialog> carInfoDialog;
    std::unique_ptr<Dialog> calendarDialog;
    std::unique_ptr<Dialog> radioDialog;
    bool rebuildRadio_ = false;
    std::unique_ptr<Dialog> stickerDialog;
    std::vector<std::unique_ptr<Bitmap>> customStickerBitmaps;
    std::vector<std::string> customStickerPaths;
    int stickerScrollRow = 0;
    bool stickerNeedsRefresh = false;
    std::vector<Car*> sortedOwnedCars;
    std::vector<SDL_Surface*> carListSwatches;
    void FreeCarListSwatches();
    std::map<Car*, std::unique_ptr<CarBitmap>> carBitmaps;
    SDL_Rect signRect;
    void InitGameMenuDialog();
    void InitOptionsDialog();
    void InitBumperDialogs();
    void ShowBumperDialog(bool isFront);
    void ShowRoofDialog();
    void ShowPaintDialog();
    void ShowCarInfoDialog();
    void ShowCalendarDialog();
    void ShowRadioDialog();
    void ShowTransmissionDialog();
    SDL_Point GetCalendarDateHighlightPosition(int month, int day);
    void ShowStickerDialog();
    void LoadCustomStickers();
    void ShowModificationDialog(const char* title, int price, std::function<void()> onConfirm, std::unique_ptr<Dialog>& dialogPtr);
    void UpdateSign();
    void CycleNextColor();
    void CyclePreviousColor();
    void ApplyPreviewColor();
    void UpdatePaintCanPreview();
    void UpdateCarZones();
    // Whether a zone click should be handled right now, i.e. no scene fade,
    // no tire-change animation, and no car-switch animation in progress.
    bool CanInteract() const { return sceneTransition == SCENE_SOLID && tireChangePhase == TireChangePhase::IDLE && !animatingCar; }

   public:
    Garage();
    ~Garage();

    void InitCarBitmaps();
    CarBitmap* GetOrCreateCarBitmap(Car* car);

    SDL_Point GetCarIdlePosition() override;

    bool PushEvent(SDL_Event* event) override;
    void DrawScene() override;
    void SetActiveDialog(Dialog *dialog) override;
    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
