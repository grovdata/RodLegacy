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
    rowHeight = SFont_TextHeight(config.font);
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
        itemRect.y = listConfig.listBoxRect.y + (i - scrollOffset) * rowHeight;
        
        SDL_Color textColor;
        if (static_cast<int>(i) == selectedIndex) {
            textColor = listConfig.selectedTextColor;
        } else if (static_cast<int>(i) == hoverIndex) {
            textColor = {0xFF, 0xFF, 0xFF, 0xFF};  // White for hover
        } else {
            textColor = {0x80, 0x80, 0x80, 0xFF};  // Gray for normal
        }
        
        SFont_Write(mainBitmap->GetSurface(), config.font, itemRect.x, itemRect.y, 
                   listConfig.labels[i].c_str());
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), textColor.r, textColor.g, textColor.b);
        mainBitmap->UpdateFromSurface();
    }

    if (selectedIndex != -1) {
        // Clear selected item area first
        SDL_Rect clearRect = listConfig.selectedItemRect;
        SDL_FillRect(mainBitmap->GetSurface(), &clearRect, 0);
        mainBitmap->UpdateFromSurface();

        // Draw centered selected text
        int textWidth = SFont_TextWidth(config.font, listConfig.labels[selectedIndex].c_str());
        int xOffset = (listConfig.selectedItemRect.w - textWidth) / 2;
        SFont_Write(mainBitmap->GetSurface(), config.font, 
                   listConfig.selectedItemRect.x + xOffset, 
                   listConfig.selectedItemRect.y, 
                   listConfig.labels[selectedIndex].c_str());
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), 
                              listConfig.selectedTextColor.r, 
                              listConfig.selectedTextColor.g, 
                              listConfig.selectedTextColor.b);
        mainBitmap->UpdateFromSurface();
    }
}

void ListDialog::PopulateList() {
    
}

void ListDialog::OnItemSelected(int index) {
    if (listConfig.onItemSelected && index >= 0) {
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

    // Handle mouse motion for hover effect
    if (event->type == SDL_MOUSEMOTION) {
        hoverIndex = GetIndexAtPosition(event->motion.x, event->motion.y);
    }

    // Handle list clicks
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        int clickedIndex = GetIndexAtPosition(event->button.x, event->button.y);
        if (clickedIndex >= 0) {
            selectedIndex = clickedIndex;
            // Don't call OnItemSelected here - wait for button press
        }
    }

    // Handle escape key
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Hide();
        }
    }
}

int ListDialog::GetIndexAtPosition(int x, int y) const {
    if (x >= listConfig.listBoxRect.x && 
        x <= listConfig.listBoxRect.x + listConfig.listBoxRect.w &&
        y >= listConfig.listBoxRect.y) {
        
        int relativeY = y - listConfig.listBoxRect.y;
        int index = scrollOffset + (relativeY / rowHeight);
        
        if (index >= 0 && index < static_cast<int>(listConfig.labels.size())) {
            return index;
        }
    }
    return -1;
}