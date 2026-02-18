#pragma once
#include "dialog.h"
#include <functional>

struct ScrollButtonsConfig {
    SDL_Rect topRect;
    SDL_Rect upRect;
    SDL_Rect downRect;
    SDL_Rect bottomRect;
};

struct ListDialogConfig {
    std::vector<std::string> labels;
    std::vector<SDL_Surface*> swatches;
    std::function<void(int)> onItemSelected;
    SDL_Rect listBoxRect;
    SDL_Rect selectedItemRect;
    SDL_Color normalTextColor = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color selectedTextColor = {0xFF, 0xFF, 0x00, 0xFF};
    ScrollButtonsConfig scrollButtons;
};

class ListDialog : public Dialog {
private:
    int selectedIndex = -1;
    int hoverIndex = -1;
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