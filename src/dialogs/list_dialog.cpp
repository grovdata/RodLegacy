#include "list_dialog.h"
#include "../state.h"
#include "../SFont.h"
#include "../ui/main_font.h"
#include "../ui/button.h"

ListDialog::ListDialog(DialogConfig config, ListDialogConfig listConfig)
    : Dialog(config),         
      listConfig(listConfig)
{
    // The rows are written into mainBitmap, so it has to be blitted even for
    // a list whose config carries no text, content areas or lines of its own.
    mainBitmapHasContent = true;
    rowHeight = SFont_TextHeight(config.font);
    // How many rows actually fit, rather than a fixed 10 - a taller list box
    // used to draw 10 rows and leave the rest of itself blank, and a shorter
    // one used to overflow its own border.
    if (rowHeight > 0 && listConfig.listBoxRect.h > 0) {
        visibleItems = SDL_max(1, listConfig.listBoxRect.h / rowHeight);
    }

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

    const int lastVisible =
        SDL_min(scrollOffset + visibleItems, (int)listConfig.labels.size());

    // Draw list items
    for (int i = scrollOffset; i < lastVisible; i++) {
        const int rowY = listConfig.listBoxRect.y + (i - scrollOffset) * rowHeight;

        SFont_Write(mainBitmap->GetSurface(), config.font,
                    listConfig.listBoxRect.x, rowY, listConfig.labels[i].c_str());

        // Draw swatch if provided
        if (!listConfig.swatches.empty() && i < static_cast<int>(listConfig.swatches.size()) && listConfig.swatches[i]) {
            SDL_Surface* swatch = listConfig.swatches[i];
            int swatchX = listConfig.listBoxRect.x + listConfig.listBoxRect.w - swatch->w - 2;
            SDL_Rect dstRect = {swatchX, rowY, swatch->w, swatch->h};
            SDL_BlitSurface(swatch, nullptr, mainBitmap->GetSurface(), &dstRect);
        }
    }

    // The centred read-out of the current pick is optional: a caller with no
    // room for it (or whose list already highlights the selection clearly
    // enough) leaves selectedItemRect empty.
    const bool hasReadout = listConfig.selectedItemRect.w > 0 &&
                            listConfig.selectedItemRect.h > 0;
    if (selectedIndex != -1 && hasReadout) {
        SDL_Rect readoutRect = listConfig.selectedItemRect;
        SDL_FillRect(mainBitmap->GetSurface(), &readoutRect, 0);

        int textWidth = SFont_TextWidth(config.font, listConfig.labels[selectedIndex].c_str());
        int xOffset = (listConfig.selectedItemRect.w - textWidth) / 2;
        SFont_Write(mainBitmap->GetSurface(), config.font,
                    listConfig.selectedItemRect.x + xOffset,
                    listConfig.selectedItemRect.y,
                    listConfig.labels[selectedIndex].c_str());
    }

    // One texture rebuild for the whole list, not one per row. This also has
    // to come after every write: UpdateFromSurface recreates the texture, so
    // a colour mod set before it is thrown away - which is why the per-row
    // colours this used to set never appeared.
    mainBitmap->UpdateFromSurface();
    if (listConfig.textColor.a > 0) {
        SDL_SetTextureColorMod(mainBitmap->GetTexture(), listConfig.textColor.r,
                               listConfig.textColor.g, listConfig.textColor.b);
    }

    // Selection box, drawn straight to the renderer so the list's colour mod
    // does not tint it.
    if (listConfig.selectionBoxColor.a > 0 &&
        selectedIndex >= scrollOffset && selectedIndex < lastVisible) {
        SDL_Rect box = {listConfig.listBoxRect.x - 1,
                        listConfig.listBoxRect.y + (selectedIndex - scrollOffset) * rowHeight - 1,
                        listConfig.listBoxRect.w + 2,
                        rowHeight + 1};
        Uint8 r, g, b, a;
        SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
        SDL_SetRenderDrawColor(renderer, listConfig.selectionBoxColor.r,
                               listConfig.selectionBoxColor.g,
                               listConfig.selectionBoxColor.b,
                               listConfig.selectionBoxColor.a);
        SDL_RenderDrawRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
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

void ListDialog::SetSelectedIndex(int index) {
    const int count = static_cast<int>(listConfig.labels.size());
    if (index < 0 || index >= count) {
        selectedIndex = -1;
        return;
    }
    selectedIndex = index;
    // Only the visible window is drawn, so a row picked past its end has to
    // be scrolled to or the highlight lands on nothing.
    if (index < scrollOffset) {
        scrollOffset = index;
    } else if (index >= scrollOffset + visibleItems) {
        scrollOffset = index - visibleItems + 1;
    }
    if (scrollOffset < 0) scrollOffset = 0;
}

SDL_Rect ListDialog::GetRowRect(int index) const {
    if (index < scrollOffset || index >= scrollOffset + visibleItems ||
        index >= static_cast<int>(listConfig.labels.size())) {
        return {0, 0, 0, 0};
    }
    return {listConfig.listBoxRect.x,
            listConfig.listBoxRect.y + (index - scrollOffset) * rowHeight,
            listConfig.listBoxRect.w, rowHeight};
}

void ListDialog::OnItemSelected(int index) {
    if (listConfig.onItemSelected && index >= 0) {
        listConfig.onItemSelected(index);
    }
    Hide();
} 

bool ListDialog::HandleEvent(SDL_Event *event) {
    bool handled = Dialog::HandleEvent(event);
    if (!IsVisible()) return handled;

    // Handle mouse motion
    if (event->type == SDL_MOUSEMOTION) {
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
            handled = true;
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
            handled = true;
            if (selectedIndex < static_cast<int>(listConfig.labels.size()) - 1) {
                selectedIndex++;
                // Ensure the selected item is visible
                if (selectedIndex >= scrollOffset + visibleItems) {
                    scrollOffset = selectedIndex - visibleItems + 1;
                }
            }
        }
    }
    return handled;
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