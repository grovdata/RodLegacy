#pragma once

#include "dialogs/dialog.h"
#include "dialogs/list_dialog.h"
#include "state.h"
#include <functional>

// Define callback types for consistency
using ButtonCallback = std::function<void(int)>;
using ZoneHoverCallback = std::function<void(Zone&)>;
using ZoneClickCallback = std::function<void(Zone&)>;

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_TUNING,
    ENGINE_BAY_PARTS
};

// Which lib.p body bitmap represents each part - shared with
// garage/garage_engine_bay_install.cpp, which needs the same mapping to
// swap bitmaps when a part is installed. See engine_bay.cpp for sourcing.
AmigaLibAsset EngineBodyAsset(CarEngine_t type);
AmigaLibAsset ManifoldBodyAsset(ManifoldType type);
AmigaLibAsset CarbBodyAsset(CarburetorType type);

// A spec-based car (never had this part explicitly fitted) has no concrete
// Part/EnginePart for engine/manifold - resolve from the spec class instead
// of reading garbage. See engine_bay.cpp for why carb doesn't need this.
class Car;
ManifoldType ResolveManifoldType(const Car& car);
CarEngine_t ResolveEngineType(const Car& car);

class EngineBay {
private:
    Dialog* engineDialog;
    Dialog* tuneDialog;
    std::unique_ptr<ListDialog> partsDialog;
    State* state;
    EngineBayState currentState;
    int tuneLevel = 0;
    float engineSpeed = 0.0f;

    // Part-mount screws (0=engine, 1=manifold-left, 2=manifold-right,
    // 3=carb) - independent ENGINE_BOLT copies, see the constructor's
    // "screw_N" bitmap setup. Owned here since EngineBay builds engineDialog;
    // Garage (garage_engine_bay_install.cpp) owns the actual swap state
    // machine and drives these through engineDialog's public GetBitmap().
    std::unique_ptr<Bitmap> screwBitmap[4];
    SDL_Point screwNaturalSize[4]{};

    // partSlot 0=engine, 1=manifold, 2=carb. Sized from whichever asset is
    // in the slot right now, since the tables are bottom-anchored.
    SDL_Rect GetPartRect(int partSlot);

public:
    EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr,
              ZoneClickCallback zoneClickCallback = nullptr);
    ~EngineBay();
    
    Dialog* GetDialog();
    void SwitchToState(EngineBayState newState);
    void ResetToOverview();
    EngineBayState GetCurrentState() const { return currentState; }
    void OnDialogShown(); // Call this after dialog->Show() to restore correct visibility
    // Re-syncs the engine/manifold/carb overview bitmaps to whichever car is
    // now current. The constructor only sets these once (for whatever car
    // was current when EngineBay was built) - switching to a different
    // owned car or loading a save otherwise leaves the overview showing
    // stale parts (e.g. always the 2-barrel carb the starter car shipped
    // with) until the player manually swaps something in that slot.
    void RefreshPartsForCurrentCar();
    // Mount point of a screw on the current car, from engine_bay_layout.h.
    // screwIndex 0=engine, 1=manifold-left, 2=manifold-right, 3=carb.
    SDL_Point GetScrewAnchor(int screwIndex);
    void SetKnobPosition(int level);
    void InitPartsDialog();
    SDL_Point GetScrewNaturalSize(int screwIndex) const { return screwNaturalSize[screwIndex]; }
    Bitmap* GetScrewBitmap(int screwIndex) const { return screwBitmap[screwIndex].get(); }
};