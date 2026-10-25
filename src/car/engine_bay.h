/*
 * RodLegacy - The engine bay window: EngineBay (overview, tuning, parts) and the read-only EngineBayView.
 *
 * Created: 2025-04-27
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "dialogs/dialog.h"
#include "dialogs/list_dialog.h"
#include "car/engine_bay_layout.h"
#include "core/state.h"
#include <functional>

using ButtonCallback = std::function<void(int)>;
using ZoneHoverCallback = std::function<void(Zone&)>;
using ZoneClickCallback = std::function<void(Zone&)>;

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_TUNING,
    ENGINE_BAY_PARTS
};

// Shared with garage_engine_bay_install.cpp, which swaps bitmaps on install.
AmigaLibAsset EngineBodyAsset(CarEngine_t type);
AmigaLibAsset ManifoldBodyAsset(ManifoldType type, CarEngine_t engine);
AmigaLibAsset CarbBodyAsset(CarburetorType type);

class Car;

class EngineBay {
private:
    Dialog* engineDialog;
    Dialog* tuneDialog;
    std::unique_ptr<ListDialog> partsDialog;
    State* state;
    EngineBayState currentState;

    bool bayDronePlaying = false;
    void UpdateBayDrone(bool wanted);
    int tuneLevel = 0;
    float engineSpeed = 0.0f;

    // Owned here; Garage drives the swap state machine through engineDialog's GetBitmap().
    std::unique_ptr<Bitmap> screwBitmap[kEngineBayScrewCount];
    SDL_Point screwNaturalSize[kEngineBayScrewCount]{};

    // Each mount needs its own Bitmap: SetVisible on a shared asset would hide them all.
    std::unique_ptr<Bitmap> carbBitmap[3];

    // The engine being fitted, whose block sets the wire and screw spots before the car has it.
    CarEngine_t pendingEngine{};
    bool hasPendingEngine = false;

public:
    EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr,
              ZoneClickCallback zoneClickCallback = nullptr);
    ~EngineBay();
    
    Dialog* GetDialog();
    void SwitchToState(EngineBayState newState);
    void ResetToOverview();
    EngineBayState GetCurrentState() const { return currentState; }
    void OnDialogShown();
    // The constructor only syncs the car current at build time.
    void RefreshPartsForCurrentCar();

    // Slots past the manifold's carburetor count exist but are never shown.
    static std::string GetPartBitmapId(int partSlot);
    SDL_Rect GetPartRect(int partSlot);
    bool IsPartFitted(int partSlot);
    bool IsPartSlotUsed(int partSlot);
    void SetPartAsset(int partSlot, AmigaLibAsset asset);
    void SetPartVisible(int partSlot, bool visible);
    SDL_Point GetScrewAnchor(int screwIndex);
    void SetPendingEngine(CarEngine_t type);
    void ClearPendingEngine() { hasPendingEngine = false; }
    static int GetScrewCount(int partSlot);
    static int GetFirstScrew(int partSlot);
    void SetKnobPosition(int level);
    void InitPartsDialog();
    SDL_Point GetScrewNaturalSize(int screwIndex) const { return screwNaturalSize[screwIndex]; }
    Bitmap* GetScrewBitmap(int screwIndex) const { return screwBitmap[screwIndex].get(); }
};

// Read-only engine bay for the diner's "pop the hood" on a challenger's car. Any click or Escape calls onClose.
class EngineBayView {
public:
    EngineBayView(Car& car, State& state, std::function<void()> onClose);
    Dialog* GetDialog() { return dialog.get(); }

private:
    std::unique_ptr<Dialog> dialog;
};