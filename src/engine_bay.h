#pragma once

#include "dialogs/dialog.h"
#include "state.h"
#include <functional>

// Define callback types for consistency
using ButtonCallback = std::function<void(int)>;
using ZoneHoverCallback = std::function<void(Zone&)>;

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_TUNING
};

class EngineBay {
private:
    Dialog* engineDialog;
    Dialog* tuneDialog;
    State* state;
    EngineBayState currentState;

public:
    EngineBay(State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr);
    ~EngineBay();
    
    Dialog* GetDialog();
    void SwitchToState(EngineBayState newState);
    void ResetToOverview();
    EngineBayState GetCurrentState() const { return currentState; }
    void OnDialogShown(); // Call this after dialog->Show() to restore correct visibility
};