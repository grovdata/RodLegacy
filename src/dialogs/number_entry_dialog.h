#pragma once

#include "dialogs/dialog.h"

#include <string>

class NumberEntryDialog : public Dialog {
   public:
    NumberEntryDialog(const DialogConfig& cfg, const std::string& prompt,
                      SDL_Rect fieldRect, int maxDigits, SDL_Color textColor);

    void Draw() override;
    bool HandleEvent(SDL_Event* event) override;
    void Show() override;
    void Hide() override;

    // 0 if nothing typed. Overlong input wraps at 16 bits in CarHaggle, not here.
    int Value() const;
    void ClearEntry() { digits.clear(); }

   private:
    std::string prompt;
    SDL_Rect fieldRect;
    int maxDigits;
    SDL_Color textColor;
    std::string digits;
};
