#pragma once

#include "dialogs/dialog.h"
#include <functional>

class DialogFactory {
public:
    static Dialog* CreateDialog(State& state);
};

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_PART_SELECT,
    ENGINE_BAY_TUNING
};

// Callback type for EngineBay button events
using ButtonCallback = std::function<void(int buttonIndex)>;

class EngineBay : public DialogFactory {
public:
    static Dialog* CreateDialog(State& state, ButtonCallback buttonCallback = nullptr);
};