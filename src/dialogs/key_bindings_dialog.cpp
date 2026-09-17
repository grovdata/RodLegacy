#include "dialogs/key_bindings_dialog.h"

#include "core/global.h"
#include "ui/control_icons.h"
#include "ui/main_font.h"

#include <vector>

namespace {

using icons::Cell;
using icons::Sheet;

// ---------------------------------------------------------------------------
// Icon sheet cell indices. index = row * 12 + col on each 12-wide sheet.
//
// KEYBOARD (12x7): rows 0-2 are the keys in keyboard order (Q W E R T Y U I O P
// A S / D F G H J K L Z X C V B / N M then 0-9), row 3 a second 0-9 set, row 4
// the modifiers and specials, row 5 backspace then F1-F11, row 6 F12.
//
// CONTROLLER (12x6): row 0 is A B X Y, the shoulders, the triggers and the four
// D-pad directions; row 1 is the stick clicks, Menu/View, the two analog sticks
// and the D-pad as one glyph. Later rows are the PlayStation and Nintendo
// equivalents, which this screen does not use.
// ---------------------------------------------------------------------------

// Keyboard letters.
constexpr int kKeyD = 12, kKeyF = 13, kKeyG = 14;

// Keyboard modifiers and specials (row 4), then backspace and the F-keys.
constexpr int kKeyCtrl = 48;
constexpr int kKeyEscape = 52;
constexpr int kKeyEnter = 53;
constexpr int kKeySpace = 54;
constexpr int kKeyArrowLeft = 55, kKeyArrowRight = 56;
constexpr int kKeyArrowUp = 57,   kKeyArrowDown = 58;

// Controller, row 0.
constexpr int kPadA = 0, kPadB = 1, kPadX = 2;
constexpr int kPadLB = 4, kPadRB = 5, kPadLT = 6, kPadRT = 7;

// Controller, row 1. Start is the Menu button on an Xbox-style pad.
constexpr int kPadStart = 14;
constexpr int kPadStick = 17;   // left analog stick
constexpr int kPadDpad = 18;    // the D-pad as one glyph

struct Row {
    const char* label;
    std::vector<Cell> keys;
    std::vector<Cell> pad;
    bool heading = false;    // drawn centred between two rules, no icons
};

Cell K(int i) { return {Sheet::Keyboard, i}; }
Cell P(int i) { return {Sheet::Controller, i}; }

std::vector<Row> BuildRows() {
    return {
        {"MENUS", {}, {}, true},
        {"Move the cursor",     {}, {P(kPadStick)}},
        {"Next button",         {}, {P(kPadDpad)}},
        {"Click",               {}, {P(kPadA)}},
        {"Back / close dialog", {K(kKeyEscape)}, {P(kPadB)}},
        {"Snap to nearest",     {}, {P(kPadX)}},
        {"Confirm",             {K(kKeyEnter)}, {P(kPadStart)}},
        {"Go to the garage",    {K(kKeyG)}, {P(kPadLB)}},
        {"Go to the diner",     {K(kKeyD)}, {P(kPadRB)}},

        {"DRIVING", {}, {}, true},
        {"Throttle", {K(kKeyArrowUp)},   {P(kPadRT), P(kPadA)}},
        {"Brake",    {K(kKeyArrowDown)}, {P(kPadLT), P(kPadB)}},
        {"Steer",    {K(kKeyArrowLeft), K(kKeyArrowRight)},
                     {P(kPadStick), P(kPadDpad)}},
        {"Shift",    {K(kKeySpace)},     {P(kPadX), P(kPadRB)}},
        {"Skip the drive", {K(kKeySpace)}, {P(kPadA)}},

        {"WINDOW", {}, {}, true},
        {"Fullscreen",   {K(kKeyCtrl), K(kKeyF)}, {}},
        {"Gamepad mode", {K(kKeyCtrl), K(kKeyG)}, {}},
    };
}

// Icon strips outlive the surfaces handed to ListDialog, so they are owned here
// and freed together.
std::vector<SDL_Surface*> g_strips;

// The icons are a white cap or button body with the lettering and outline in
// black. Painting the body the dialog's own grey sinks it into the background,
// and the line art goes white so it reads on top of that grey.
const SDL_Color kIconBody = {0x90, 0x90, 0x90, 0xFF};
const SDL_Color kIconLine = {0xd1, 0xd1, 0xd1, 0xFF};

// A section heading: the title centred on the row with a rule running out to
// each edge. Built as a row-wide surface and handed to ListDialog as the row's
// swatch, because ListDialog writes every label hard against the left edge and
// this is the only list that wants anything else.
SDL_Surface* HeadingStrip(SFont_Font* font, const char* text, int w, int h) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    // Opaque dialog grey, like the icons: the row underneath is that grey, and
    // an opaque patch avoids blending onto a transparent destination.
    SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, kIconBody.r, kIconBody.g,
                                         kIconBody.b, 0xFF));

    const int textW = SFont_TextWidth(font, text);
    const int textX = (w - textW) / 2;
    SFont_Write(s, font, textX, (h - SFont_TextHeight(font)) / 2, text);

    // Same white as the divider under the title.
    const Uint32 rule = SDL_MapRGBA(s->format, 0xFF, 0xFF, 0xFF, 0xFF);
    const int ruleY = h / 2;
    const int gap = 4;
    const int rightX = textX + textW + gap;
    SDL_Rect left  = {0, ruleY, textX - gap, 1};
    SDL_Rect right = {rightX, ruleY, w - rightX, 1};
    if (left.w > 0) SDL_FillRect(s, &left, rule);
    if (right.w > 0) SDL_FillRect(s, &right, rule);
    return s;
}

}  // namespace

void FreeKeyBindingsIcons() {
    for (SDL_Surface* s : g_strips) {
        if (s) SDL_FreeSurface(s);
    }
    g_strips.clear();
}

std::unique_ptr<ListDialog> MakeKeyBindingsDialog(SFont_Font* font,
                                                  std::function<void()> onBack) {
    FreeKeyBindingsIcons();

    const std::vector<Row> rows = BuildRows();

    // The icons are 16px tall, so the rows have to be taller than the font
    // would otherwise make them or the artwork overlaps the row below.
    const int kRowHeight = 18;

    SDL_Rect bounds = {0, 0, GAME_WIDTH, GAME_HEIGHT};

    // Standard grey list-dialog geometry: the list on the left, a column of
    // scroll buttons down the right edge, the dismiss button underneath.
    const int kScrollW = 15;
    const int kListTop = 20;
    const int kListBottom = GAME_HEIGHT - 20;
    const int kVisibleRows = (kListBottom - kListTop) / kRowHeight;

    SDL_Rect listBox = {6, kListTop, GAME_WIDTH - 12 - kScrollW - 4,
                        kVisibleRows * kRowHeight};

    std::vector<std::string> labels;
    std::vector<SDL_Surface*> swatches;
    labels.reserve(rows.size());
    swatches.reserve(rows.size());

    for (const Row& r : rows) {
        if (r.heading) {
            // The whole heading is the artwork; there is no label to write.
            labels.push_back("");
            SDL_Surface* s = HeadingStrip(font, r.label, listBox.w - 2, kRowHeight);
            g_strips.push_back(s);
            swatches.push_back(s);
            continue;
        }

        labels.push_back(r.label);

        // One strip per row: the keyboard icons, then the pad icons after a
        // blank cell so the two sides read as separate columns.
        std::vector<Cell> strip = r.keys;
        if (!r.keys.empty() && !r.pad.empty()) {
            strip.push_back(K(-1));  // skipped by Strip(), leaves a gap
        }
        strip.insert(strip.end(), r.pad.begin(), r.pad.end());

        SDL_Surface* s = strip.empty() ? nullptr
                                       : icons::Strip(strip, kIconBody, kIconLine);
        g_strips.push_back(s);
        swatches.push_back(s);
    }

    const int scrollX = listBox.x + listBox.w + 4;
    SDL_Rect scrollTopRect    = {scrollX, listBox.y, kScrollW, kScrollW};
    SDL_Rect scrollUpRect     = {scrollX, listBox.y + 17, kScrollW, kScrollW};
    SDL_Rect scrollDownRect   = {scrollX, listBox.y + listBox.h - 32, kScrollW, kScrollW};
    SDL_Rect scrollBottomRect = {scrollX, listBox.y + listBox.h - 15, kScrollW, kScrollW};

    SDL_Rect backRect = {(GAME_WIDTH - 66) / 2, GAME_HEIGHT - 18, 66, 16};

    DialogConfig cfg;
    cfg.bounds = bounds;
    cfg.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    cfg.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    cfg.contentAreas = std::vector<SDL_Rect>{bounds};
    cfg.font = font;
    cfg.texts = {
        DialogTextConfig{SDL_Rect{bounds.x + 3, bounds.y + 4, bounds.w - 6, 10},
                         "Controls", {0x00, 0x00, 0x00, 0xFF}, true},
    };
    cfg.lines = {{bounds.x + 4, 12, bounds.w - 8, 1}};

    ButtonConfig backButton = {backRect, "Forget it", nullptr, true, 0};
    backButton.cancelChoice = true;
    cfg.buttons = {backButton};
    cfg.onButtonPressed = [onBack](int index) {
        if (index == 0 && onBack) onBack();
    };

    ListDialogConfig listCfg;
    listCfg.labels = labels;
    listCfg.swatches = swatches;
    listCfg.listBoxRect = listBox;
    listCfg.rowHeight = kRowHeight;
    listCfg.scrollButtons = {scrollTopRect, scrollUpRect, scrollDownRect, scrollBottomRect};

    return std::make_unique<ListDialog>(cfg, listCfg);
}
