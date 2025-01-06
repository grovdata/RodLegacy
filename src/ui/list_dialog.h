#pragma once
#include "dialog.h"
#include <functional>

class ListDialog : public Dialog {
private:
    std::vector<std::string> labels;
    int selectedIndex = -1;
    int scrollOffset = 0;
    int visibleItems = 10;
    Button* scrollUpBtn;
    Button* scrollDownBtn;
    std::function<void(int)> onItemSelected;

    void PopulateList();
public:
    ListDialog(const SDL_Rect& bounds, std::vector<std::string> labels, std::function<void(int)> callback);
    ~ListDialog();

    void Draw() override;
    void HandleEvent(SDL_Event* event);

    void OnItemSelected(int index);
    int GetSelectedIndex() const { return selectedIndex; }
}; 