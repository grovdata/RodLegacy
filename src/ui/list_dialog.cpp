#include "list_dialog.h"
#include "../state.h"

ListDialog::ListDialog(const SDL_Rect& bounds, std::vector<std::string> labels, std::function<void(int)> onItemSelected)
    : Dialog(bounds),
      labels(labels),
      onItemSelected(onItemSelected)
{

    SDL_Rect topRect = {37, 27, 246, 95};
    AddContentArea(topRect);
    
    SDL_Rect bottomRect = {37, 123, 246, 40};
    AddContentArea(bottomRect);
    
    //SDL_Rect listBoxRect = {3, listRowHeight + 6, 242, 80};    
    
    int buttonWidth = 66;
    int buttonHeight = 16;

    Button* switchItButton = new Button(
        "Switch it",
        SDL_Rect{mainBounds.x + 14, mainBounds.y + mainBounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() {
            if (selectedIndex >= 0) {
                this->OnItemSelected(selectedIndex);
            }
        }
    );

    Button* sellItButton = new Button(
        "Sell it",
        SDL_Rect{mainBounds.x + 14 + buttonWidth + 2, mainBounds.y + mainBounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() {
            // TODO: Implement sell logic
        }
    );
    
    Button* cancelBtn = new Button(
        "Forget it",
        SDL_Rect{mainBounds.x + 14 + buttonWidth*2 + 2*2, mainBounds.y + mainBounds.h - buttonHeight - 2, buttonWidth, buttonHeight},
        [this]() { Hide(); }
    );
    
    AddButton(switchItButton);
    AddButton(sellItButton);
    AddButton(cancelBtn);    

    
    Button* scrollTopBtn = new Button(
        "\\",
        SDL_Rect{topRect.x + topRect.w + 2, topRect.y + 14, 15, 15},
        [this]() {
            //if (scrollOffset > 0) scrollOffset--;
        }
    );

    Button* scrollUpBtn = new Button(
        "<",
        SDL_Rect{topRect.x + topRect.w + 2, topRect.y + 30, 15, 15},
        [this]() {
            //if (scrollOffset > 0) scrollOffset--;
        }
    );
    
    Button* scrollDownBtn = new Button(
        ">", 
        SDL_Rect{topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15},
        [this]() {
            // if (scrollOffset + visibleItems < static_cast<int>(labels.size())) scrollOffset++;
        }
    );

    Button* scrollBottomBtn = new Button(
        "/",
        SDL_Rect{topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15},
        [this]() {
            //if (scrollOffset > 0) scrollOffset--;
        }
    );
    
    AddButton(scrollTopBtn);
    AddButton(scrollUpBtn);
    AddButton(scrollDownBtn);
    AddButton(scrollBottomBtn);
    
    PopulateList();
}

ListDialog::~ListDialog() {
    labels.clear();
}

void ListDialog::Draw() {
    Dialog::Draw();
}

void ListDialog::PopulateList() {
    
}

void ListDialog::OnItemSelected(int index) {
    if (onItemSelected) {
        onItemSelected(index);
    }
    Hide();
} 

void ListDialog::HandleEvent(SDL_Event *event) {
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Hide();
        }
    }
}