#include "gas_station.h"
#include "state.h"

GasStation::GasStation() : CarScene("../data/res/lib_p_340.gif") {
    // SDL_Rect bottleRect = {7, 161, 8, 24};
    // Zone bottle(ZONE_GARAGE_DRINK, strdup("Wanna drink ?"), &bottleRect);
    // zones.push_back(bottle);
}

MousePointer GasStation::GetPointerType() {
    return PointerHand;
}

SDL_Point GasStation::GetCarIdlePosition() {
    return {35, 82};
}

SDL_Point GasStation::GetCarOffscreenPosition() {
    return {-320, 82};
}

void GasStation::Action(ZoneId zone) {
    if (zone == ZONE_GARAGE_GOTO_DINER) {
        state.MoveToScene(SCENE_DINER);
    }
}