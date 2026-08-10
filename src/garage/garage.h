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
#include "part.h"
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
    std::unique_ptr<ListDialog> transmissionDialog;
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

    // Transmission install animation (garage/garage_transmission_install.cpp).
    // Mirrors the tire-change state machine above: MOUNTED is the steady
    // state (screws visually "in"); UNSCREWING/SCREWING_IN are click-driven
    // (2 clicks per screw, see docs/garage/transmission_install.md); SLIDING_IN
    // is the only timer-driven phase. The car's actual gearbox is never
    // mutated until SCREWING_IN completes, so backing out mid-animation via
    // Done just resets the visuals - no rollback logic needed.
    enum class TransInstallPhase {
        MOUNTED,
        UNSCREWING,
        EMPTY,
        SLIDING_IN,
        SCREWING_IN,
    };
    TransInstallPhase transInstallPhase = TransInstallPhase::MOUNTED;
    float transInstallTimer = 0.0f;
    int transScrewState[2] = {0, 0};  // 0 = seated .. 2 = fully backed out
    Part pendingGearbox;
    SDL_Point transSlideStart{};
    SDL_Point transSlideMid{};
    SDL_Point transSlideEnd{};
    SDL_Point transNaturalSize{};  // unscaled transmission bitmap size, for re-deriving the clip each frame
    std::unique_ptr<Dialog> transmissionInstallDialog;
    std::unique_ptr<Bitmap> transScrewBitmap[2];  // independent copies of ENGINE_TRANSMISSION_BOLT, cropped per state

    // Once a screw finishes backing out it drops out of the window; once a
    // new transmission is in place, fresh screws drop in from the top. Runs
    // independently of transInstallPhase/transScrewState (which stay
    // click-driven and immediate) - this is a purely cosmetic Y translation
    // layered on top.
    bool transScrewFalling[2] = {false, false};
    bool transScrewFallingOut[2] = {false, false};  // true = dropping out (hide on arrival); false = dropping in from the top
    float transScrewFallTimer[2] = {0.0f, 0.0f};
    int transScrewFallFromY[2] = {0, 0};
    int transScrewFallToY[2] = {0, 0};

    void ShowTransmissionInstallDialog();
    void UpdateTransmissionInstallAnimation(double deltaTime);
    void OnTransScrewClicked(int screwIndex);
    void UpdateTransScrewVisual(int screwIndex);
    void StartScrewFall(int screwIndex, bool fallingOut);
    void UpdateScrewFallAnimations(double deltaTime);
    void StartTransmissionSlideIn(const Part& newGearbox);
    void CommitTransmissionSwap();
    void ResetTransmissionInstallToMounted();
    DialogBitmap BuildTransmissionBitmapEntry(TransmissionType type);

    // Read-only readout of state's spare engines/transmissions/manifolds/
    // carburetors (bought from the newspaper, not yet installed). Selling is
    // tracked separately (TODO.md's "Selling parts").
    std::unique_ptr<ListDialog> sparePartsDialog;
    void ShowSparePartsDialog();

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

    // Rebuilds every cache keyed on Car* (bitmaps, swatches, the sign) after
    // State's owned-car set has been replaced out from under Garage, e.g. by
    // LoadSaveGame or a new game. If Garage is the scene on screen right now,
    // also resets it to the same state a fresh scene entry leaves behind so
    // the (possibly absent) current car drives in normally instead of
    // popping into place.
    void ResetForNewCarSet();

    SDL_Point GetCarIdlePosition() override;

    bool PushEvent(SDL_Event* event) override;
    void DrawScene() override;
    void SetActiveDialog(Dialog *dialog) override;
    void StartAnimateCar(Car* car, Driver* driver, CarAnimationType animType) override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
