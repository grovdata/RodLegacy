#include "list_dialog.h"
#include "../state.h"
#include "../SFont.h"

ListDialog::ListDialog(DialogConfig config, ListDialogConfig listConfig)
    : Dialog(config),         
      listConfig(listConfig)
{
    rowHeight = SFont_TextHeight(config.font);

    ButtonConfig topConfig;
    topConfig.text = "\\";
    topConfig.bounds = listConfig.scrollButtons.topRect;
    topConfig.onClick = [this]() {
        scrollOffset = 0;
    };
    scrollTopBtn = new Button(topConfig);

    ButtonConfig upConfig;
    upConfig.text = "<";
    upConfig.bounds = listConfig.scrollButtons.upRect;
    upConfig.onClick = [this]() {
        if (scrollOffset > 0) scrollOffset--;
    };
    scrollUpBtn = new Button(upConfig);
    
    ButtonConfig downConfig;
    downConfig.text = ">";
    downConfig.bounds = listConfig.scrollButtons.downRect;
    downConfig.onClick = [this]() {
        if (scrollOffset + visibleItems < static_cast<int>(this->listConfig.labels.size())) 
            scrollOffset++;
    };
    scrollDownBtn = new Button(downConfig);

    ButtonConfig bottomConfig;
    bottomConfig.text = "/";
    bottomConfig.bounds = listConfig.scrollButtons.bottomRect;
    bottomConfig.onClick = [this]() {
        scrollOffset = static_cast<int>(this->listConfig.labels.size()) - visibleItems;
        if (scrollOffset < 0) scrollOffset = 0;
    };
    scrollBottomBtn = new Button(bottomConfig);
    
    // Add scroll buttons to Dialog's button list
    buttons.push_back(scrollTopBtn);
    buttons.push_back(scrollUpBtn);
    buttons.push_back(scrollDownBtn);
    buttons.push_back(scrollBottomBtn);

    PopulateList();
}

ListDialog::~ListDialog() {
    
}

void ListDialog::Draw() {
    Dialog::Draw();
    
    if (!IsVisible()) return;

    // Clear list box area
    SDL_Rect clearRect = listConfig.listBoxRect;
    SDL_FillRect(mainBitmap->GetSurface(), &clearRect, 0);
    mainBitmap->UpdateFromSurface();

    // Draw list items
    for (size_t i = scrollOffset; i < SDL_min(scrollOffset + visibleItems, (int)listConfig.labels.size()); i++) {
        SDL_Rect itemRect = listConfig.selectedItemRect;
        itemRect.y = listConfig.listBoxRect.y + (i - scrollOffset) * rowHeight;
        
        SDL_Color textColor;
        if (static_cast<int>(i) == selectedIndex) {
            textColor = listConfig.selectedTextColor;
        } else if (static_cast<int>(i) == hoverIndex) {
            textColor = {255, 255, 255, 255};  // White for hover
        } else {
            textColor = {0, 0, 0, 255};  // Gray for normal
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
        // Check if click is within listbox bounds
        if (event->button.x >= listConfig.listBoxRect.x && 
            event->button.x <= listConfig.listBoxRect.x + listConfig.listBoxRect.w &&
            event->button.y >= listConfig.listBoxRect.y &&
            event->button.y <= listConfig.listBoxRect.y + listConfig.listBoxRect.h) {
            
            int clickedIndex = GetIndexAtPosition(event->button.x, event->button.y);
            if (clickedIndex >= 0) {
                selectedIndex = clickedIndex;
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