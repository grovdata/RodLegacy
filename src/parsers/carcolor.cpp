#include "carcolor.h"
#include <SDL.h>

 vector<uint16_t> GetStandardCarColor(StandardCarColor col) {
    switch (col) {
        case CarColorRed:
            return vector<uint16_t>(PAL_CAR_COLOR_RED);
        case CarColorCyan:
            return vector<uint16_t>(PAL_CAR_COLOR_CYAN);
        case CarColorLightGreen:
            return vector<uint16_t>(PAL_CAR_COLOR_LIGHT_GREEN);
        case CarColorMagenta:
            return vector<uint16_t>(PAL_CAR_COLOR_MAGENTA);
        case CarColorNavy:
            return vector<uint16_t>(PAL_CAR_COLOR_NAVY);
        case CarColorGrey:
            return vector<uint16_t>(PAL_CAR_COLOR_GREY);
        case CarColorPurple:
            return vector<uint16_t>(PAL_CAR_COLOR_PURPLE);
        case CarColorTeal:
            return vector<uint16_t>(PAL_CAR_COLOR_TEAL);
        case CarColorGoldenBrown:
            return vector<uint16_t>(PAL_CAR_COLOR_GOLDEN_BROWN);
        case CarColorMaroon:
            return vector<uint16_t>(PAL_CAR_COLOR_MAROON);
        case CarColorOrange:
            return vector<uint16_t>(PAL_CAR_COLOR_ORANGE);
        case CarColorDarkGreen:
            return vector<uint16_t>(PAL_CAR_COLOR_DARK_GREEN);
        case CarColorTurquoise:
            return vector<uint16_t>(PAL_CAR_COLOR_TURQUOISE);
        default:
            return vector<uint16_t>();
    }
}
