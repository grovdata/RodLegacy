/*
 * RodLegacy - Debug scene for checking car wheel lines and driver positions (debug_ui builds only).
 *
 * Created: 2026-08-08
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "car/car.h"
#include "car/carbitmap.h"
#include "ui/button.h"
#include "ui/label.h"

#include <map>
#include <memory>
#include <vector>

// Debug scene for checking one car model's wheel line and driver position against the art.
//
//   < >  or Left/Right   previous / next model
//   Up / Down            raise / lower the wheels by one pixel
//   I / K                raise / lower the driver
//   J / L                move the driver left / right
//   P                    next driver portrait
//   0                    reset this model to its stock offsets
//   D                    dump every adjusted model to the log
class CarDebug : public Scene {
private:
    std::vector<CarModel> models;
    int currentIndex = 0;

    CarPtr car;
    std::unique_ptr<CarBitmap> carBitmap;
    std::unique_ptr<Button> prevButton;
    std::unique_ptr<Button> nextButton;
    std::unique_ptr<Button> dumpButton;

    std::map<int, int> wheelYAdjust;
    std::map<int, SDL_Point> driverAdjust;

    Driver debugDriver;
    int portraitIndex = 0;

    Label titleLabel;
    Label spriteLabel;
    Label wheelLabel;
    Label driverLabel;

    void LoadModels();
    // The King's car is never for sale so CarData builds no model for it; synthesise one per set.
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
