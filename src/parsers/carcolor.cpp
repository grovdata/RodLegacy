#include "carcolor.h"
#include <SDL.h>

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
        default:
            // Never an empty palette: every caller indexes the result
            // directly (BitmapParser::PrepareCarPalette reads 9 entries), so
            // returning nothing here is an out-of-bounds read rather than a
            // missing colour. Save files carry a raw colour index, which
            // makes this reachable from data rather than only from a bug.
            return std::vector<uint16_t>(PAL_CAR_COLOR_RED);
    }
}
