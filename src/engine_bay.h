#pragma once

#include "dialogs/dialog.h"

class DialogFactory {
public:
    static Dialog* CreateDialog(State& state);
};

enum EngineBayState {
    ENGINE_BAY_OVERVIEW,
    ENGINE_BAY_PART_SELECT,
    ENGINE_BAY_TUNING
};

class EngineBay : public DialogFactory {
public:
    static Dialog* CreateDialog(State& state);
};