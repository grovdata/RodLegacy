#include "list_dialog.h"
#include "../state.h"
#include "../SFont.h"

ListDialog::ListDialog(DialogConfig config, ListDialogConfig listConfig)
    : Dialog(config),         
      listConfig(listConfig)
{

    // int buttonWidth = 66;
    // int buttonHeight = 16;

    // Button* switchItButton = new Button(
    //     "Switch it",
    //     SDL_Rect{bounds.x + 14, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
    //     [this]() {
    //         this->config.onButtonPressed(0);
    //     }
    // );

    // Button* sellItButton = new Button(
    //     "Sell it",
    //     SDL_Rect{bounds.x + 14 + buttonWidth + 2, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
    //     [this]() {
    //         this->config.onButtonPressed(1);
    //     }
    // );
    
    // Button* cancelBtn = new Button(
    //     "Forget it",
    //     SDL_Rect{bounds.x + 14 + buttonWidth*2 + 2*2, bounds.y + bounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
    //     [this]() { 
    //         this->config.onButtonPressed(2);
    //         this->Hide();
    //     }
    // );
    
    // AddButton(switchItButton);
    // AddButton(sellItButton);
    // AddButton(cancelBtn);    

    
    // Button* scrollTopBtn = new Button(
    //     "\\",
    //     SDL_Rect{topRect.x + topRect.w + 2, topRect.y + 14, 15, 15},
    //     [this]() {
    //         //if (scrollOffset > 0) scrollOffset--;
    //     }
    // );

    // Button* scrollUpBtn = new Button(
    //     "<",
    //     SDL_Rect{topRect.x + topRect.w + 2, topRect.y + 30, 15, 15},
    //     [this]() {
    //         //if (scrollOffset > 0) scrollOffset--;
    //     }
    // );
    
    // Button* scrollDownBtn = new Button(
    //     ">", 
    //     SDL_Rect{topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15},
    //     [this]() {
    //         // if (scrollOffset + visibleItems < static_cast<int>(labels.size())) scrollOffset++;
    //     }
    // );

    // Button* scrollBottomBtn = new Button(
    //     "/",
    //     SDL_Rect{topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15},
    //     [this]() {
    //         //if (scrollOffset > 0) scrollOffset--;
    //     }
    // );
    
    // AddButton(scrollTopBtn);
    // AddButton(scrollUpBtn);
    // AddButton(scrollDownBtn);
    // AddButton(scrollBottomBtn);
    
    PopulateList();
}

ListDialog::~ListDialog() {

}

void ListDialog::Draw() {
    Dialog::Draw();
    
    if (!IsVisible()) return;

    // Draw list items
    for (size_t i = scrollOffset; i < SDL_min(scrollOffset + visibleItems, (int)listConfig.labels.size()); i++) {
        SDL_Rect itemRect = listConfig.selectedItemRect;
        itemRect.y = listConfig.listBoxRect.y + (i - scrollOffset) * itemRect.h;
        
        SDL_Color textColor = (static_cast<int>(i) == selectedIndex) 
            ? listConfig.selectedTextColor 
            : listConfig.normalTextColor;
        
        SFont_Write(mainBitmap->GetSurface(), config.font, itemRect.x, itemRect.y, 
                          listConfig.labels[i].c_str());
    }
}

void ListDialog::PopulateList() {
    
}

void ListDialog::OnItemSelected(int index) {
    if (listConfig.onItemSelected) {
        listConfig.onItemSelected(index);
    }
    Hide();
} 

void ListDialog::HandleEvent(SDL_Event *event) {
    if (!IsVisible()) return;

    // Handle button events
    for (auto button : buttons) {
        button->HandleEvent(event);
    }

    // Handle list clicks
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        int x = event->button.x;
        int y = event->button.y;
        
        if (x >= listConfig.listBoxRect.x && 
            x <= listConfig.listBoxRect.x + listConfig.listBoxRect.w &&
            y >= listConfig.listBoxRect.y) {
            
            int clickedIndex = scrollOffset + (y - listConfig.listBoxRect.y) / listConfig.selectedItemRect.h;
            if (clickedIndex >= 0 && clickedIndex < static_cast<int>(listConfig.labels.size())) {
                selectedIndex = clickedIndex;
                OnItemSelected(selectedIndex);
            }
        }
    }

    // Handle escape key
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Hide();
        }
    }
}