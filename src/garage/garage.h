/*
 * RodLegacy - The garage scene: Garage with its menus, paint, tires, transmission and engine bay installs,
 * and car sales.
 *
 * Created: 2023-02-26
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "core/global.h"
#include "scenes/scene.h"
#include "core/mouse.h"
#include "ui/zone.h"
#include "ui/bubble.h"
#include "dialogs/list_dialog.h"
#include "dialogs/number_entry_dialog.h"
#include "car/car_haggle.h"
#include "car/carbitmap.h"
#include "car/engine_bay.h"
#include "gfx/animated_bitmap.h"
#include "parsers/carcolor.h"
#include "car/part.h"
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
    // A spare carries its wear and ad name, so fitting it must not hand over a fresh set.
    Part pendingTire;
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
    void StartTireChangeAnimation(const Part& newTire);
    void UpdateTireChangeAnimation(double deltaTime);

    // MOUNTED is the steady state; UNSCREWING/SCREWING_IN are click-driven (2 clicks per screw) and SLIDING_IN
    // is timed. The gearbox is only changed when SCREWING_IN completes, so backing out needs no rollback.
    enum class TransInstallPhase {
        MOUNTED,
        UNSCREWING,
        EMPTY,
        SLIDING_IN,
        SCREWING_IN,
    };
    TransInstallPhase transInstallPhase = TransInstallPhase::MOUNTED;
    // A wrong-make pick still animates in before bouncing out.
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
    // Cached at construction: Dialog::Draw's no-crop path stomps the bitmap's rect to 0x0 once a clip is empty.
    SDL_Point transScrewNaturalSize[2]{};

    // Purely cosmetic Y translation, independent of the click-driven state.
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
    // The one place crop, X push-out and porthole clip combine, at rest and every fall frame.
    void ApplyScrewGeometry(int screwIndex, int y);
    void StartScrewFall(int screwIndex, bool fallingOut, float delay = 0.0f);
    void UpdateScrewFallAnimations(double deltaTime);
    void StartTransmissionSlideIn(const Part& newGearbox);
    void CommitTransmissionSwap();
    void ResetTransmissionInstallToMounted();
    DialogBitmap BuildTransmissionBitmapEntry(TransmissionType type);

    // Engine bay part swap: engine (1 screw), manifold (2), carbs (1 each, up to 3), flat-indexed
    // 0=engine, 1=manifold-left, 2=manifold-right, 3..5=carb 0..2. Screws grow upwards as they back out.
   public:
    // Public for the install code's free functions. A dual-quad or triple manifold carries separate carbs.
    enum class EngineBaySlot { ENGINE = 0, MANIFOLD = 1, CARB0 = 2, CARB1 = 3, CARB2 = 4 };
    enum class PartInstallPhase { MOUNTED, UNSCREWING, EMPTY, SLIDING_IN, SCREWING_IN };
   private:
    PartInstallPhase engineBaySlotPhase[kEngineBayPartSlots] = {
        PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED,
        PartInstallPhase::MOUNTED, PartInstallPhase::MOUNTED
    };
    // A part that does not fit still animates in before it is rejected.
    bool engineBaySlotRejected[kEngineBayPartSlots]{};
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

    // A second precondition, besides its screw, for the ENGINE slot to count as out or in.
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
    // seating = the part dropping in (low, loud); otherwise a screw turning (high, quieter).
    void PlayBayToolSound(bool seating);
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

    // The one place the spare list order is defined, so a row index means the same to the list and the sale.
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
    // Reopening from inside its own row callback would destroy the running dialog, so it is deferred a frame.
    bool sparePartsNeedsReopen = false;
    void ShowSparePartsDialog();
    void SellSparePart(int row);

    CarPtr pendingCar;
    // Tells an already parked car apart from one that must drive in.
    CarPtr lastParkedCar;
    void OnSceneExit() override;
    SDL_Surface *signSurface;
    SDL_Texture *signTexture;
    std::unique_ptr<ListDialog> carSelectDialog;

    // The buyer's haggle lines and the parts sign-off. Appended last in the constructor.
    static constexpr size_t kSellerBubble = 14;
    // Shown entering a garage with no car, nothing to sell and no money; holds until a key or click.
    static constexpr size_t kOutOfDoughBubble = 15;
    bool outOfDough = false;

    // Fuel gates, all on fuel tenths.
    static constexpr size_t kNotEnoughGasBubble = 16;  // fuel < 9, going to the diner
    static constexpr size_t kFullTankBubble     = 17;  // fuel >= 140, going to the gas station
    static constexpr size_t kGasNotFreeBubble   = 18;  // fuel < 140 and bankroll == 0, same trip

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
    // Its newspaper list price.
    int CarSaleValue(const CarPtr& car);

    std::unique_ptr<Dialog> gameMenuDialog;
    std::unique_ptr<Dialog> optionsDialog;
    std::unique_ptr<ListDialog> keyBindingsDialog;
    std::unique_ptr<ListDialog> settingsDialog;
    std::unique_ptr<Dialog> frontBumperDialog;
    std::unique_ptr<Dialog> rearBumperDialog;
    std::unique_ptr<Dialog> roofDialog;
    std::unique_ptr<Dialog> paintDialog;
    std::unique_ptr<EngineBay> engineBay;
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
    void DropUnownedCarBitmaps();
    std::map<CarPtr, std::unique_ptr<CarBitmap>> carBitmaps;
    SDL_Rect signRect;
    void InitGameMenuDialog();
    void InitOptionsDialog();
    void ShowKeyBindingsDialog();
    void ShowSettingsDialog();
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
