#pragma once

#include "scene.h"
#include "car.h"
#include "carbitmap.h"
#include "ui/button.h"
#include "ui/label.h"

#include <map>
#include <memory>
#include <vector>

// Debug-only scene for eyeballing one car model at a time and correcting its
// wheel line. Built only when -Ddebug_ui=true.
//
// The per-model wheel Y (BitmapOffset_t::offsetYTires) has no source in the
// game data: the Amiga table CAR_WHEELS_Y_OFFSETS was measured by hand and is
// 30 for most cars, and DOS car sets start at that same 30 for every car
// because their data file has no equivalent field. This scene is how those
// numbers get corrected.
//
//   < >  or Left/Right   previous / next model
//   Up / Down            raise / lower the wheels by one pixel
//   0                    reset this model to its stock offset
//   D                    dump every adjusted model to the log
//
// Adjustments are kept per model while the scene is open, so several cars can
// be tuned in one pass and read off together with D.
class CarDebug : public Scene {
private:
    std::vector<CarModel> models;
    int currentIndex = 0;

    std::unique_ptr<Car> car;
    std::unique_ptr<CarBitmap> carBitmap;
    std::unique_ptr<Button> prevButton;
    std::unique_ptr<Button> nextButton;
    std::unique_ptr<Button> dumpButton;

    // Model index -> nudge applied on top of the model's stock offsetYTires.
    std::map<int, int> wheelYAdjust;

    Label titleLabel;
    Label spriteLabel;
    Label wheelLabel;

    void LoadModels();
    // The King's car is spec slot 25 in every set and is never for sale, so
    // CarData builds no model for it. It still has art and a wheel line, so
    // the scene synthesises one per set.
    void AppendKingModels();
    void RefreshLabels();
    void ShowModel(int index);
    void Cycle(int delta);
    void AdjustWheelY(int delta);
    void ResetWheelY();
    void DumpAdjustments() const;
    int CurrentAdjust() const;
    int StockWheelY() const;

public:
    CarDebug();
    ~CarDebug() = default;

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event* event) override;
    void UpdateDebugInfo() override;
};
