#include "list_dialog.h"
#include "../state.h"
#include "../SFont.h"
#include "../ui/main_font.h"
#include "../ui/button.h"

ListDialog::ListDialog(DialogConfig config, ListDialogConfig listConfig)
    : Dialog(config),         
      listConfig(listConfig)
{
    rowHeight = SFont_TextHeight(config.font);

    ButtonConfig topConfig;
    topConfig.text = std::string(1, MAIN_FONT_SCROLL_TOP);
    topConfig.bounds = listConfig.scrollButtons.topRect;
    topConfig.fillColor = config.backgroundColor;
    topConfig.borderColor = config.borderColor;
    topConfig.textColor = ButtonDefaultChoiceColor;//config.borderColor;
    topConfig.onClick = [this]() {
        scrollOffset = 0;
    };
    scrollTopBtn = new Button(topConfig);

    ButtonConfig upConfig;
    upConfig.text = std::string(1, MAIN_FONT_SCROLL_UP);
    upConfig.bounds = listConfig.scrollButtons.upRect;
    upConfig.fillColor = config.backgroundColor;
    upConfig.borderColor = config.borderColor;
    upConfig.textColor = ButtonDefaultChoiceColor;//config.borderColor;
    upConfig.onClick = [this]() {
        if (scrollOffset > 0) scrollOffset--;
    };
    scrollUpBtn = new Button(upConfig);
    
    ButtonConfig downConfig;
    downConfig.text = std::string(1, MAIN_FONT_SCROLL_DOWN);
    downConfig.bounds = listConfig.scrollButtons.downRect;
    downConfig.fillColor = config.backgroundColor;
    downConfig.borderColor = config.borderColor;
    downConfig.textColor = ButtonDefaultChoiceColor;//config.borderColor;
    downConfig.onClick = [this]() {
        if (scrollOffset + visibleItems < static_cast<int>(this->listConfig.labels.size())) 
            scrollOffset++;
    };
    scrollDownBtn = new Button(downConfig);

    ButtonConfig bottomConfig;
    bottomConfig.text = std::string(1, MAIN_FONT_SCROLL_BOTTOM);
    bottomConfig.bounds = listConfig.scrollButtons.bottomRect;
    bottomConfig.fillColor = config.backgroundColor;
    bottomConfig.borderColor = config.borderColor;
    bottomConfig.textColor = ButtonDefaultChoiceColor;//config.borderColor;
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
    for (int i = scrollOffset; i < SDL_min(scrollOffset + visibleItems, (int)listConfig.labels.size()); i++) {
        SDL_Rect itemRect = listConfig.selectedItemRect;
        itemRect.x = listConfig.listBoxRect.x;
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

        // Draw swatch if provided
        if (!listConfig.swatches.empty() && i < static_cast<int>(listConfig.swatches.size()) && listConfig.swatches[i]) {
            SDL_Surface* swatch = listConfig.swatches[i];
            int swatchX = listConfig.listBoxRect.x + listConfig.listBoxRect.w - swatch->w - 2;
            int swatchY = itemRect.y;
            SDL_Rect dstRect = {swatchX, swatchY, swatch->w, swatch->h};
            SDL_BlitSurface(swatch, nullptr, mainBitmap->GetSurface(), &dstRect);
            mainBitmap->UpdateFromSurface();
        }
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

void ListDialog::SetLabels(const std::vector<std::string>& labels, const std::vector<SDL_Surface*>& swatches) {
    listConfig.labels = labels;
    listConfig.swatches = swatches;
    selectedIndex = -1;
    scrollOffset = 0;
}

void ListDialog::OnItemSelected(int index) {
    if (listConfig.onItemSelected && index >= 0) {
        listConfig.onItemSelected(index);
    }
    Hide();
} 

void ListDialog::HandleEvent(SDL_Event *event) {
    Dialog::HandleEvent(event);
    if (!IsVisible()) return;

    // Handle mouse motion
    if (event->type == SDL_MOUSEMOTION) {
        hoverIndex = GetIndexAtPosition(event->motion.x, event->motion.y);
        mousePos.x = event->motion.x;
        mousePos.y = event->motion.y;
    }

    // Handle mousewheel scrolling when mouse is over listbox
    if (event->type == SDL_MOUSEWHEEL) {
        
        int wheelY = event->wheel.y;

        if (mousePos.x >= listConfig.listBoxRect.x && 
            mousePos.x <= listConfig.listBoxRect.x + listConfig.listBoxRect.w &&
            mousePos.y >= listConfig.listBoxRect.y &&
            mousePos.y <= listConfig.listBoxRect.y + listConfig.listBoxRect.h) {
            
            if (wheelY > 0) { // Scroll up
                if (scrollOffset > 0) {
                    scrollOffset--;
                }
            }
            else if (wheelY < 0) { // Scroll down
                if (scrollOffset + visibleItems < static_cast<int>(listConfig.labels.size())) {
                    scrollOffset++;
                }
            }
        }
    }

    // Handle list clicks
    if (event->type == SDL_MOUSEBUTTONDOWN) {
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

    // Handle keyboard events
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            Hide();
        }
        else if (event->key.keysym.sym == SDLK_RETURN && selectedIndex >= 0) {
            // If Enter is pressed and an item is selected, trigger the selection
            OnItemSelected(selectedIndex);
        }
        else if (event->key.keysym.sym == SDLK_UP) {
            // Move selection up
            if (selectedIndex > 0) {
                selectedIndex--;
                // Ensure the selected item is visible
                if (selectedIndex < scrollOffset) {
                    scrollOffset = selectedIndex;
                }
            }
        }
        else if (event->key.keysym.sym == SDLK_DOWN) {
            // Move selection down
            if (selectedIndex < static_cast<int>(listConfig.labels.size()) - 1) {
                selectedIndex++;
                // Ensure the selected item is visible
                if (selectedIndex >= scrollOffset + visibleItems) {
                    scrollOffset = selectedIndex - visibleItems + 1;
                }
            }
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