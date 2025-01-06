#pragma once
#include "dialog.h"
#include <functional>

using namespace std;

struct ListDialogConfig {  
    vector<string> labels;
    function<void(int)> onItemSelected;
    SDL_Rect listBoxRect;
    SDL_Rect selectedItemRect;
};

class ListDialog : public Dialog {
private:
    int selectedIndex = -1;
    int scrollOffset = 0;
    int visibleItems = 10;
    Button* scrollUpBtn;
    Button* scrollDownBtn;
    Button* scrollTopBtn;
    Button* scrollBottomBtn;

    ListDialogConfig listConfig;

    void PopulateList();
public:
    ListDialog(DialogConfig config, ListDialogConfig listConfig);
    ~ListDialog();

    void Draw() override;
    void HandleEvent(SDL_Event* event);

    void OnItemSelected(int index);
    int GetSelectedIndex() const { return selectedIndex; }
}; 