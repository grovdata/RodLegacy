#include "dialogs/number_entry_dialog.h"

#include "gfx/bitmap.h"

#include <cstdlib>

NumberEntryDialog::NumberEntryDialog(const DialogConfig& cfg, const std::string& prompt,
                                     SDL_Rect fieldRect, int maxDigits, SDL_Color textColor)
    : Dialog(cfg),
      prompt(prompt),
      fieldRect(fieldRect),
      maxDigits(maxDigits),
      textColor(textColor) {
    mainBitmapHasContent = true;
}

int NumberEntryDialog::Value() const {
    if (digits.empty()) return 0;
    return std::atoi(digits.c_str());
}

void NumberEntryDialog::Show() {
    Dialog::Show();
    SDL_StartTextInput();
}

void NumberEntryDialog::Hide() {
    SDL_StopTextInput();
    Dialog::Hide();
}

bool NumberEntryDialog::HandleEvent(SDL_Event* event) {
    if (!IsVisible()) return false;

    if (event->type == SDL_TEXTINPUT) {
        for (const char* c = event->text.text; *c != '\0'; c++) {
            if (*c >= '0' && *c <= '9' && (int)digits.size() < maxDigits) {
                digits.push_back(*c);
            }
        }
        return true;
    }
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_BACKSPACE) {
        if (!digits.empty()) digits.pop_back();
        return true;
    }

    return Dialog::HandleEvent(event);
}

void NumberEntryDialog::Draw() {
    Dialog::Draw();
    if (!IsVisible()) return;

    SDL_Rect clearRect = fieldRect;
    SDL_FillRect(mainBitmap->GetSurface(), &clearRect, 0);

    // The font has no cursor glyph.
    const std::string line = prompt + digits + "_";
    SFont_Write(mainBitmap->GetSurface(), config.font, fieldRect.x, fieldRect.y, line.c_str());

    // UpdateFromSurface recreates the texture, so the colour mod must come after it.
    mainBitmap->UpdateFromSurface();
    SDL_SetTextureColorMod(mainBitmap->GetTexture(), textColor.r, textColor.g, textColor.b);
}
