#pragma once

#include "dialogs/dialog.h"
#include "dialogs/list_dialog.h"
#include "state.h"
#include <functional>

// Define callback types for consistency
using ButtonCallback = std::function<void(int)>;
using ZoneHoverCallback = std::function<void(Zone&)>;

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_TUNING,
    ENGINE_BAY_PARTS
};

class EngineBay {
private:
    Dialog* engineDialog;
    Dialog* tuneDialog;
    std::unique_ptr<ListDialog> partsDialog;
    State* state;
    EngineBayState currentState;
    int tuneLevel = 0;
    float engineSpeed = 0.0f;

public:
    EngineBay(int tuneLevel, State& state, ButtonCallback buttonCallback, ZoneHoverCallback zoneHoverCallback = nullptr);
    ~EngineBay();
    
    Dialog* GetDialog();
    void SwitchToState(EngineBayState newState);
    void ResetToOverview();
    EngineBayState GetCurrentState() const { return currentState; }
    void OnDialogShown(); // Call this after dialog->Show() to restore correct visibility
    void SetKnobPosition(int level);
    void InitPartsDialog();
};