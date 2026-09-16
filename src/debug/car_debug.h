#pragma once

#include "scenes/scene.h"
#include "car/car.h"
#include "car/carbitmap.h"
#include "ui/button.h"
#include "ui/label.h"

#include <map>
#include <memory>
#include <vector>

// Debug-only scene for eyeballing one car model at a time and correcting its
// wheel line. Built only when -Ddebug_ui=true.
//
// The per-model wheel Y (BitmapOffset_t::offsetYTires) has no field in the
// game data: it is derived, bottom-aligning the wheel cell to the body sprite
// (CarSetWheelY). This scene is how that is checked against the art.
//
// It also places a driver portrait, anchored by its bottom edge on the
// per-car y_driver from SpriteInfo.
//
//   < >  or Left/Right   previous / next model
//   Up / Down            raise / lower the wheels by one pixel
//   I / K                raise / lower the driver
//   J / L                move the driver left / right
//   P                    next driver portrait
//   0                    reset this model to its stock offsets
//   D                    dump every adjusted model to the log
//
// Adjustments are kept per model while the scene is open, so several cars can
// be tuned in one pass and read off together with D.
class CarDebug : public Scene {
private:
    std::vector<CarModel> models;
    int currentIndex = 0;

    CarPtr car;
    std::unique_ptr<CarBitmap> carBitmap;
    std::unique_ptr<Button> prevButton;
    std::unique_ptr<Button> nextButton;
    std::unique_ptr<Button> dumpButton;

    // Model index -> nudge applied on top of the model's stock offsetYTires.
    std::map<int, int> wheelYAdjust;
    // Model index -> nudge applied on top of the driver's computed position.
    std::map<int, SDL_Point> driverAdjust;

    Driver debugDriver;
    int portraitIndex = 0;

    Label titleLabel;
    Label spriteLabel;
    Label wheelLabel;
    Label driverLabel;

    void LoadModels();
    // The King's car is spec slot 25 in every set and is never for sale, so
    // CarData builds no model for it. It still has art and a wheel line, so
    // the scene synthesises one per set.
    void AppendKingModels();
    void RefreshLabels();
    void ShowModel(int index);
    void Cycle(int delta);
    void AdjustWheelY(int delta);
    void AdjustDriver(int dx, int dy);
    void CyclePortrait();
    void ResetModel();
    void DumpAdjustments() const;
    int CurrentAdjust() const;
    int StockWheelY() const;
    SDL_Point CurrentDriverAdjust() const;

public:
    CarDebug();
    ~CarDebug() = default;

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void UpdateDebugInfo() override;
};
