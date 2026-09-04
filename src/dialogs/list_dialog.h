#pragma once
#include "dialog.h"
#include <functional>

// Zeroed by default: a caller that wants no scroll buttons (SaveLoadScene)
// just leaves these alone, and default-initialising a ListDialogConfig on the
// stack can't hand Rectangle garbage dimensions to build a surface from.
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

    // Kept last so existing positional aggregate initialisation of this
    // struct keeps compiling.
    // Tint applied to the whole list once it is rendered. Alpha 0 leaves the
    // font's own colours alone, which is what every caller got before - the
    // per-row colours above never reached the screen, because the texture was
    // recreated (dropping the colour mod) immediately after each one was set.
    SDL_Color textColor = {0, 0, 0, 0};

    // Outline drawn around the selected row. Alpha 0 draws nothing. Use this
    // rather than a per-row text colour to mark the selection: a colour mod
    // applies to the entire list texture, so one row cannot differ from
    // the rest.
    SDL_Color selectionBoxColor = {0, 0, 0, 0};
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
    void HandleEvent(SDL_Event* event) override;

    void OnItemSelected(int index);
    int GetSelectedIndex() const { return selectedIndex; }
    int GetIndexAtPosition(int x, int y) const;
    void SetLabels(const std::vector<std::string>& labels, const std::vector<SDL_Surface*>& swatches = {});
}; 