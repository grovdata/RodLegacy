#pragma once

#include "global.h"
#include "scene.h"
#include "mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "dialogs/list_dialog.h"
#include "dialogs/number_entry_dialog.h"
#include "car_haggle.h"
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
    // Picked a transmission of the wrong make: it still animates in before
    // bouncing back out, same as the engine bay slots.
    bool transRejected = false;
    float transInstallTimer = 0.0f;
    int transScrewState[2] = {0, 0};  // 0 = seated .. 2 = fully backed out
    Part pendingGearbox;
    SDL_Point transSlideStart{};
    SDL_Point transSlideMid{};
    SDL_Point transSlideEnd{};
    SDL_Point transNaturalSize{};  // unscaled transmission bitmap size, for re-deriving the clip each frame
    std::unique_ptr<Dialog> transmissionInstallDialog;
    std::unique_ptr<Bitmap> transScrewBitmap[2];  // independent copies of ENGINE_TRANSMISSION_BOLT, cropped per state
    // Cached once at construction - transScrewBitmap[i]->GetPosition() is
    // NOT safe to re-query later: whenever a screw's clip goes to {0,0,0,0}
    // (fully outside the porthole), Dialog::Draw()'s no-crop fallback path
    // calls SetPosition/SetDimensions on the underlying Bitmap, permanently
    // stomping its own rect to that same 0x0 - see ApplyScrewGeometry.
    SDL_Point transScrewNaturalSize[2]{};

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
    float transScrewFallDelay[2] = {0.0f, 0.0f};  // counts down while waiting for its turn, so the two drop in one after the other

    void ShowTransmissionInstallDialog();
    void UpdateTransmissionInstallAnimation(double deltaTime);
    void OnTransScrewClicked(int screwIndex);
    void UpdateTransScrewVisual(int screwIndex);
    // Sets a screw's crop (from its click-state), X (pushed out by the same
    // distance as the crop growth), and porthole clip, at the given Y - the
    // one place all three combine, used both at rest and every fall frame.
    void ApplyScrewGeometry(int screwIndex, int y);
    void StartScrewFall(int screwIndex, bool fallingOut, float delay = 0.0f);
    void UpdateScrewFallAnimations(double deltaTime);
    void StartTransmissionSlideIn(const Part& newGearbox);
    void CommitTransmissionSwap();
    void ResetTransmissionInstallToMounted();
    DialogBitmap BuildTransmissionBitmapEntry(TransmissionType type);

    // Engine bay part-swap animation (garage/garage_engine_bay_install.cpp).
    // Same idea as the transmission install above, but 3 slots sharing
    // EngineBay's own overview dialog instead of a dedicated popup: engine
    // (1 screw), manifold (2 screws), carb (1 per carburetor the manifold
    // mounts, 1 to 3) - 6 screws total, flat-indexed 0=engine,
    // 1=manifold-left, 2=manifold-right, 3..5=carb 0..2.
    // Like the transmission's screws these grow as they back out, but
    // upwards out of the mount rather than sideways - see ScrewFrameCrop.
   public:
    // Public so garage_engine_bay_install.cpp's free functions (ScrewSlot,
    // SlotScrewCount, etc.) can name them; the members using these types
    // stay private below.
    // One slot per drawn part, matching the original's car struct: a
    // dual-quad or triple manifold really does carry two or three separate
    // carburetors, each fitted and pulled on its own.
    enum class EngineBaySlot { ENGINE = 0, MANIFOLD = 1, CARB0 = 2, CARB1 = 3, CARB2 = 4 };
    enum class PartInstallPhase { MOUNTED, UNSCREWING, EMPTY, SLIDING_IN, SCREWING_IN };
   private:
    PartInstallPhase engineBaySlotPhase[kEngineBayPartSlots] = {
        PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED,
        PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED
    };
    // Set when a slide-in was started for a part that doesn't fit the car's
    // current engine (e.g. a triple-2bbl manifold on a V8) - the original
    // still animates it in before rejecting it, rather than hiding it from
    // the picker. See UpdateEngineBayInstallAnimation.
    bool engineBaySlotRejected[kEngineBayPartSlots]{};
    // Six screws: 1 engine, 2 manifold, up to 3 carburetor.
    int engineBayScrewState[kEngineBayScrewCount]{};
    bool engineBayScrewFalling[kEngineBayScrewCount]{};
    bool engineBayScrewFallingOut[kEngineBayScrewCount]{};
    float engineBayScrewFallTimer[kEngineBayScrewCount]{};
    int engineBayScrewFallFromY[kEngineBayScrewCount]{};
    int engineBayScrewFallToY[kEngineBayScrewCount]{};
    float engineBayScrewFallDelay[kEngineBayScrewCount]{};

    float engineBaySlideTimer[kEngineBayPartSlots]{};
    SDL_Point engineBaySlideStart[kEngineBayPartSlots]{};
    SDL_Point engineBaySlideEnd[kEngineBayPartSlots]{};

    // The engine's ground wire: a second, independent precondition (besides
    // its screw) for the ENGINE slot to count as fully disassembled/
    // reassembled. Single click toggles it - no multi-step crop/fall, it's
    // a straight swap between the two ENGINE_WIRE_* bitmaps.
    bool engineWireAttached = true;

    EnginePart pendingEngine;
    Part pendingManifold;
    Part pendingCarb[kMaxCarburetors];

    std::unique_ptr<ListDialog> engineSwapDialog;
    std::unique_ptr<ListDialog> manifoldSwapDialog;
    std::unique_ptr<ListDialog> carbSwapDialog;

    void OnEngineBayZoneClicked(Zone& zone);
    void UpdateEngineBayInstallAnimation(double deltaTime);
    void OnEngineBayScrewClicked(int screwIndex);
    void UpdateEngineBayScrewVisual(int screwIndex);
    void ApplyEngineBayScrewGeometry(int screwIndex, int bottomY);
    void StartEngineBayScrewFall(int screwIndex, bool fallingOut, float delay = 0.0f);
    void UpdateEngineBayScrewFallAnimations(double deltaTime);
    void OnEngineBayPartClicked(EngineBaySlot slot);
    void OnEngineWireClicked();
    void OnEngineBayPartsButtonClicked();
    bool IsEngineBaySlotFullyOut(EngineBaySlot slot);
    bool IsEngineBaySlotFullySeated(EngineBaySlot slot);
    void ReleaseEngineBayPartToSpares(EngineBaySlot slot);
    void EmptyEngineBaySlot(EngineBaySlot slot);
    void ApplyEngineBayAssemblyState();
    // Puts the ground wire's sprite in step with engineWireAttached.
    void ApplyEngineWireVisual();
    void RefreshEngineBayGeometry();
    void ApplyEngineBaySlideFrame(EngineBaySlot slot, int y, const SDL_Rect& mount);
    void StartEngineBaySlideIn(EngineBaySlot slot, AmigaLibAsset asset, const SDL_Rect& mount);
    void StartEngineSlideIn(const EnginePart& newEngine);
    void StartManifoldSlideIn(const Part& newManifold);
    void StartCarbSlideIn(int carbIndex, const Part& newCarb);
    void CommitEngineBaySwap(EngineBaySlot slot);
    void SyncEngineBayPhasesFromCar();
    void ShowEngineDialog();
    void ShowManifoldDialog();
    void ShowCarburetorDialog(int carbIndex);

    // The spare parts list (bought from the newspaper or pulled off a car),
    // in the order it is shown - and the one place that order is defined, so
    // a row index means the same thing to the list and to the sale.
    struct SpareRef {
        PartCategory category;
        int index;        // position within that category's vector
        std::string label;
        int value;
        int wear;
    };
    std::vector<SpareRef> CollectSpares() const;
    std::unique_ptr<ListDialog> sparePartsDialog;
    std::unique_ptr<Dialog> sellPartDialog;
    // Reopening the list from inside its own row callback would destroy the
    // dialog whose event handler is running, so the sale defers it a frame.
    bool sparePartsNeedsReopen = false;
    void ShowSparePartsDialog();
    void SellSparePart(int row);

    CarPtr pendingCar;
    // Car shown here when the scene was last exited (nullptr if the garage
    // was empty) - lets DrawScene tell "car was already parked, just show
    // it" apart from "this car wasn't here before, drive it in".
    CarPtr lastParkedCar;
    void OnSceneExit() override;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    std::unique_ptr<ListDialog> carSelectDialog;

    // The bubble everything buying from the player talks through - the car
    // buyer's haggle lines and the parts screen's sign-off. Appended last in
    // Garage's constructor.
    static constexpr size_t kSellerBubble = 14;
    // bubbles[kOutOfDoughBubble] - the end of the run, shown on entering a
    // garage with no car, nothing to sell and no money (State::IsOutOfDough).
    // The original shows its message box here and then rolls the hall of fame;
    // this holds the garage until a key or click moves on to it.
    static constexpr size_t kOutOfDoughBubble = 15;
    bool outOfDough = false;

    // Fuel gates confirmed against FUN_002430d2 (Amiga garage main loop):
    // messages 37/38/39, all car+0x2c (fuel tenths) checks. See gas_station.md.
    static constexpr size_t kNotEnoughGasBubble = 16;  // fuel < 9, going to the diner
    static constexpr size_t kFullTankBubble     = 17;  // fuel >= 140, going to the gas station
    static constexpr size_t kGasNotFreeBubble   = 18;  // fuel < 140 and bankroll == 0, same trip

    // Selling a car. The haggle itself is in car_haggle.h; this end owns the
    // two dialogs it drives and the car being sold. See garage_sell_car.cpp.
    std::unique_ptr<CarHaggle> haggle;
    CarPtr haggleCar;
    std::unique_ptr<NumberEntryDialog> priceEntryDialog;
    std::unique_ptr<Dialog> counterOfferDialog;
    void StartSellCar(const CarPtr& car);
    void ShowCounterOfferDialog(int amount);
    void HandleHaggleStep(const HaggleStep& step);
    void ShowHaggleMessage(HaggleMessage message);
    void FinishCarSale(int price);
    void CloseHaggle();
    // What the buyer values a car at: its newspaper list price.
    int CarSaleValue(const CarPtr& car);

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
    std::vector<CarPtr> sortedOwnedCars;
    std::vector<SDL_Surface*> carListSwatches;
    void FreeCarListSwatches();
    // Housekeeping on scene entry - see the definition for why it is only that.
    void DropUnownedCarBitmaps();
    std::map<CarPtr, std::unique_ptr<CarBitmap>> carBitmaps;
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
    // Adds the tire-change animation to CarScene::CanInteract's checks.
    bool CanInteract() const { return CarScene::CanInteract() && tireChangePhase == TireChangePhase::IDLE; }

   public:
    Garage();
    ~Garage();

    CarBitmap* GetOrCreateCarBitmap(const CarPtr& car);


    SDL_Point GetCarIdlePosition() override;

    bool PushEvent(SDL_Event* event) override;
    void DrawScene() override;
    void OnSceneEnter() override;
    void SetActiveDialog(Dialog *dialog) override;
    void StartAnimateCar(CarPtr car, Driver* driver, CarAnimationType animType) override;
    void OnCarAnimationComplete() override;
    void OnFadeComplete() override;
};
