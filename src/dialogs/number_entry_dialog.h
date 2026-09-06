#pragma once

#include "dialog.h"

#include <string>

// A dialog with one typed number in it - the original's "I'll take $____"
// price field. Digits and backspace edit it, Return is the same as pressing
// the dialog's default button.
//
// The field is redrawn on every frame into the dialog's own bitmap, the way
// ListDialog draws its rows, so the static texts in the DialogConfig are
// still written once at construction and left alone.
class NumberEntryDialog : public Dialog {
   public:
    // prompt is what sits in front of the digits ("I'll take $"), fieldRect
    // is the strip they are drawn in, and maxDigits caps how many the player
    // can type.
    NumberEntryDialog(const DialogConfig& cfg, const std::string& prompt,
                      SDL_Rect fieldRect, int maxDigits, SDL_Color textColor);

    void Draw() override;
    void HandleEvent(SDL_Event* event) override;
    void Show() override;
    void Hide() override;

    // What the player typed, 0 if nothing. Overlong input wraps at 16 bits in
    // CarHaggle, not here, so this returns the number as typed.
    int Value() const;
    void ClearEntry() { digits.clear(); }

   private:
    std::string prompt;
    SDL_Rect fieldRect;
    int maxDigits;
    SDL_Color textColor;
    std::string digits;
};
