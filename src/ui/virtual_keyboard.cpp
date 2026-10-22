#include "ui/virtual_keyboard.h"
#include "core/gamepad.h"
#include "core/global.h"
#include "core/state.h"
#include "gfx/SFont.h"
#include "scenes/scene.h"
#include <algorithm>
#include <cctype>

namespace {
const std::vector<std::vector<std::string>> ALPHA_ROWS = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Del"},
    {"Z", "X", "C", "V", "B", "N", "M", "Spc", "Shft", "OK"},
};
const std::vector<std::vector<std::string>> NUMERIC_ROWS = {
    {"1", "2", "3"}, {"4", "5", "6"}, {"7", "8", "9"}, {"Del", "0", "OK"},
};
constexpr int KEY_W = 30;
constexpr int KEY_H = 14;
constexpr int PAD = 4;
constexpr Uint32 REPEAT_MS = 200;

void SendKey(Scene* scene, SDL_Keycode key) {
    for (Uint32 type : {SDL_KEYDOWN, SDL_KEYUP}) {
        SDL_Event e;
        SDL_zero(e);
        e.key.type = type;
        e.key.timestamp = SDL_GetTicks();
        e.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
        e.key.keysym.sym = key;
        e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
        scene->PushEvent(&e);
    }
}

void SendText(Scene* scene, const std::string& text) {
    SDL_Event e;
    SDL_zero(e);
    e.text.type = SDL_TEXTINPUT;
    e.text.timestamp = SDL_GetTicks();
    SDL_strlcpy(e.text.text, text.c_str(), sizeof(e.text.text));
    scene->PushEvent(&e);
}
}

VirtualKeyboard virtualKeyboard;

bool VirtualKeyboard::Visible() {
    if (!SDL_IsTextInputActive()) hidden = false;
    return !hidden && SDL_IsTextInputActive();
}

void VirtualKeyboard::Reset() {
    rows = numeric ? NUMERIC_ROWS : ALPHA_ROWS;
    row = col = 0;
}

void VirtualKeyboard::SetNumeric(bool n) {
    numeric = n;
    Reset();
}

void VirtualKeyboard::Move(int dx, int dy) {
    if (rows.empty()) Reset();
    row = std::max(0, std::min(row + dy, (int)rows.size() - 1));
    const int n = (int)rows[row].size();
    col = dx ? (col + dx + n) % n : std::min(col, n - 1);
}

const std::string& VirtualKeyboard::Selected() const {
    return rows[row][col];
}

std::string VirtualKeyboard::Typed(const std::string& key) const {
    if (key == "Spc") return " ";
    if (key.size() != 1 || !std::isalpha((unsigned char)key[0])) return "";
    return std::string(1, shift ? std::toupper(key[0]) : std::tolower(key[0]));
}

void VirtualKeyboard::Press(Scene* scene, const std::string& key) {
    if (key == "Del") SendKey(scene, SDLK_BACKSPACE);
    else if (key == "OK") SendKey(scene, SDLK_RETURN);
    else if (key == "Shft") shift = !shift;
    else {
        const std::string typed = Typed(key);
        SendText(scene, typed.empty() ? key : typed);
    }
}

void VirtualKeyboard::Update(Scene* scene) {
    if (rows.empty()) Reset();

    int dx = 0, dy = 0;
    if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) dx = -1;
    else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) dx = 1;
    else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_UP)) dy = -1;
    else if (gamepad.Down(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) dy = 1;

    const Uint32 now = SDL_GetTicks();
    if ((dx || dy) && (dx != lastDx || dy != lastDy || now - lastMoveTime > REPEAT_MS)) {
        Move(dx, dy);
        lastMoveTime = now;
    }
    lastDx = dx;
    lastDy = dy;

    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_A)) Press(scene, Selected());
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_X)) Press(scene, "Del");
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_Y) || gamepad.Pressed(SDL_CONTROLLER_BUTTON_RIGHTSTICK)) Press(scene, "Shft");
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_B)) SendKey(scene, SDLK_ESCAPE);
    if (gamepad.Pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) Press(scene, "OK");
}

void VirtualKeyboard::Draw() {
    if (rows.empty()) Reset();
    SFont_Font* font = state.GetAssets().GetMainFont();
    const int w = (int)rows[0].size() * KEY_W + 2 * PAD;
    const int h = (int)rows.size() * KEY_H + 2 * PAD;

    SDL_Surface* s = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    auto fill = [&](SDL_Rect r, Uint8 v) { SDL_FillRect(s, &r, SDL_MapRGB(s->format, v, v, v)); };

    fill({0, 0, w, h}, 0x60);
    fill({1, 1, w - 2, h - 2}, 0xFF);
    fill({2, 2, w - 4, h - 4}, 0x60);

    for (int r = 0; r < (int)rows.size(); r++) {
        for (int c = 0; c < (int)rows[r].size(); c++) {
            const bool sel = (r == row && c == col);
            SDL_Rect cell = {PAD + c * KEY_W + 1, PAD + r * KEY_H + 1, KEY_W - 2, KEY_H - 2};
            fill(cell, sel ? 0xFF : 0x00);
            fill({cell.x + 1, cell.y + 1, cell.w - 2, cell.h - 2}, sel ? 0xB1 : 144);
            std::string label = Typed(rows[r][c]);
            if (label.empty() || label == " ") label = rows[r][c];
            const int tw = SFont_TextWidth(font, label.c_str());
            const int th = SFont_TextHeight(font);
            SDL_SetSurfaceColorMod(font->Surface, 0, 0, 0);
            SFont_Write(s, font, cell.x + (cell.w - tw) / 2, cell.y + (cell.h - th) / 2, label.c_str());
        }
    }
    SDL_SetSurfaceColorMod(font->Surface, 255, 255, 255);

    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect dst = {(GAME_WIDTH - w) / 2, GAME_HEIGHT - h, w, h};
    SDL_RenderCopy(renderer, t, nullptr, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}
