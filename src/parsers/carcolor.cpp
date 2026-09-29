#include "parsers/carcolor.h"
#include <SDL.h>

uint16_t GetCarRaceColor(StandardCarColor col) {
    static const uint16_t kRaceColors[] = {
        0x0810,  // red
        0x0188,  // cyan
        0x0180,  // light green
        0x0F0F,  // magenta   - not the ramp's 0x0818
        0x0018,  // navy
        0x0888,  // grey      - not the ramp's 0x0666
        0x0539,  // purple
        0x0066,  // teal
        0x0640,  // golden brown
        0x0633,  // maroon
        0x0830,  // orange
        0x0060,  // dark green
        0x0063,  // turquoise
        0x0023,  // The King
    };
    const int i = static_cast<int>(col);
    if (i < 0 || i >= static_cast<int>(std::size(kRaceColors))) {
        return kRaceColors[CarColorRed];
    }
    return kRaceColors[i];
}

 std::vector<uint16_t> GetStandardCarColor(StandardCarColor col) {
    switch (col) {
        case CarColorRed:
            return std::vector<uint16_t>(PAL_CAR_COLOR_RED);
        case CarColorCyan:
            return std::vector<uint16_t>(PAL_CAR_COLOR_CYAN);
        case CarColorLightGreen:
            return std::vector<uint16_t>(PAL_CAR_COLOR_LIGHT_GREEN);
        case CarColorMagenta:
            return std::vector<uint16_t>(PAL_CAR_COLOR_MAGENTA);
        case CarColorNavy:
            return std::vector<uint16_t>(PAL_CAR_COLOR_NAVY);
        case CarColorGrey:
            return std::vector<uint16_t>(PAL_CAR_COLOR_GREY);
        case CarColorPurple:
            return std::vector<uint16_t>(PAL_CAR_COLOR_PURPLE);
        case CarColorTeal:
            return std::vector<uint16_t>(PAL_CAR_COLOR_TEAL);
        case CarColorGoldenBrown:
            return std::vector<uint16_t>(PAL_CAR_COLOR_GOLDEN_BROWN);
        case CarColorMaroon:
            return std::vector<uint16_t>(PAL_CAR_COLOR_MAROON);
        case CarColorOrange:
            return std::vector<uint16_t>(PAL_CAR_COLOR_ORANGE);
        case CarColorDarkGreen:
            return std::vector<uint16_t>(PAL_CAR_COLOR_DARK_GREEN);
        case CarColorTurquoise:
            return std::vector<uint16_t>(PAL_CAR_COLOR_TURQUOISE);
        case CarColorKing:
            return std::vector<uint16_t>(PAL_CAR_COLOR_KING);
        default:
            // Never empty: callers index the result directly, and save files carry a raw colour index.
            return std::vector<uint16_t>(PAL_CAR_COLOR_RED);
    }
}
