#pragma once

#include "dialogs/dialog.h"
#include <functional>

// Define callback types for consistency
using ButtonCallback = std::function<void(int)>;
using ZoneHoverCallback = std::function<void(Zone&)>;

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_PART_SELECT,
    ENGINE_BAY_TUNING
};

class EngineBay {
public:
    static Dialog* CreateDialog(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr);
    static Dialog* CreateTuneDialog(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr);
    
private:
    static void SetupOverviewState(Dialog* dialog);
    static void SetupTuningState(Dialog* dialog);
    static void OnButtonPressed(Dialog* dialog, int buttonIndex);
    static EngineBayState currentState;
};