/*
 * RodLegacy - Scrollable list dialog: ListDialog with selection, swatches and scroll buttons.
 *
 * Created: 2025-01-06
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once
#include "dialogs/dialog.h"
#include <functional>

// Zeroed so a stack-initialised config can't build a Rectangle from garbage.
struct ScrollButtonsConfig {
    SDL_Rect topRect{};
    SDL_Rect upRect{};
    SDL_Rect downRect{};
    SDL_Rect bottomRect{};
};

struct ListDialogConfig {
    std::vector<std::string> labels;
    std::vector<SDL_Surface*> swatches;
    std::function<void(int)> onItemSelected;
    SDL_Rect listBoxRect{};
    SDL_Rect selectedItemRect{};

    ScrollButtonsConfig scrollButtons;

    // Fields from here on are kept last so positional aggregate initialisation still compiles.
    // Tint for the whole list. Alpha 0 leaves the font's colours alone.
    SDL_Color textColor = {0, 0, 0, 0};

    // Alpha 0 draws nothing. A colour mod applies to the whole list texture, so selection is an outline.
    SDL_Color selectionBoxColor = {0, 0, 0, 0};

    // Off by default: other lists are select-then-confirm.
    bool activateOnClick = false;

    // 0 follows the font height.
    int rowHeight = 0;
};

class ListDialog : public Dialog {
private:
    int selectedIndex = -1;
    int scrollOffset = 0;
    int visibleItems = 10;
    int rowHeight = 10;
    Button* scrollUpBtn;
    Button* scrollDownBtn;
    Button* scrollTopBtn;
    Button* scrollBottomBtn;

    ListDialogConfig listConfig;
    SDL_Point mousePos;

    void PopulateList();
public:
    ListDialog(DialogConfig config, ListDialogConfig listConfig);
    ~ListDialog();

    void Draw() override;
    bool HandleEvent(SDL_Event* event) override;

    void OnItemSelected(int index);
    // With activateOnClick the list stays open.
    void ActivateRow(int index);
    int GetSelectedIndex() const { return selectedIndex; }
    void SetSelectedIndex(int index);
    SDL_Rect GetRowRect(int index) const;
    int GetIndexAtPosition(int x, int y) const;
    void SetLabels(const std::vector<std::string>& labels, const std::vector<SDL_Surface*>& swatches = {});
    // Keeps selection and scroll, which SetLabels resets.
    void SetSwatch(int index, SDL_Surface* swatch);
}; 