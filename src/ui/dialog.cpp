#include "dialog.h"

#define COL_BORDER      0, 111, 239
#define COL_FILL        20, 240, 20
#define COL_HIGHLIGHT   0, 120, 130  // Lighter green for selection

Dialog::Dialog(const std::string& title, const SDL_Rect& bounds, const SFont_Font* font)
    : bounds(bounds), 
      title(title), 
      visible(false),
      font(font),
      hasListBox(false),
      selectedIndex(-1), 
      scrollOffset(0),
      scrollUpBtn(nullptr),
      scrollDownBtn(nullptr),
      surface(nullptr),
      texture(nullptr)
{
    // Calculate list box area
    int titleHeight = SFont_TextHeight(font) + 10;  // Title + padding
    listRowHeight = SFont_TextHeight(font) + 1;
    listBoxRect = {
        2,                  // x: 2 pixels from dialog edge
        titleHeight,        // y: below title area
        bounds.w - 4,       // width: dialog width minus borders
        bounds.h - titleHeight - 4  // height: remaining space minus bottom border
    };
    
    visibleItems = listBoxRect.h / listRowHeight;  // Adjust item height if needed

    surface = SDL_CreateRGBSurface(0, bounds.w, bounds.h, 32,
                                  0x00FF0000,
                                  0x0000FF00,
                                  0x000000FF,
                                  0xFF000000);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
}

Dialog::~Dialog() {
    if (surface) {
        SDL_FreeSurface(surface);
    }
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    for (auto button : buttons) {
        delete button;
    }
}

void Dialog::Show() {
    visible = true;
}

void Dialog::Hide() {
    visible = false;
}

void Dialog::EnableList() {
    hasListBox = true;
}

void Dialog::AddButton(Button* button) {
    buttons.push_back(button);
}

void Dialog::AddItem(const std::string& item) {
    items.push_back(item);
}

void Dialog::SetItems(const std::vector<std::string>& newItems) {
    items = newItems;
    selectedIndex = -1;
    scrollOffset = 0;
}

void Dialog::HandleEvent(SDL_Event* event) {
    if (!visible) return;

    // Handle button events
    for (auto button : buttons) {
        button->HandleEvent(event);
    }

    // Handle list selection if enabled
    if (hasListBox && event->type == SDL_MOUSEBUTTONDOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            int x = event->button.x - bounds.x;
            int y = event->button.y - bounds.y;
            
            // Check if click is within listBoxRect
            if (x >= listBoxRect.x && x < listBoxRect.x + listBoxRect.w && 
                y >= listBoxRect.y && y < listBoxRect.y + listBoxRect.h) {
                int clickedIndex = scrollOffset + (y - listBoxRect.y) / listRowHeight;
                if (clickedIndex < static_cast<int>(items.size())) {
                    selectedIndex = clickedIndex;
                }
            }
        }
    }
}

void Dialog::Draw() {
    if (!visible) return;

    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, COL_FILL));


    SDL_Rect border = {1, 1, bounds.w - 2, bounds.h - 2};
    SDL_FillRect(surface, &border, SDL_MapRGB(surface->format, COL_BORDER));
    
    // Draw inner area
    SDL_Rect inner = {2, 2, bounds.w - 4, bounds.h - 4};
    SDL_FillRect(surface, &inner, SDL_MapRGB(surface->format, COL_FILL));

    // Draw title
    int textHeight = SFont_TextHeight(font);
    int textWidth = SFont_TextWidth(font, title.c_str());
    SFont_Write(surface, font, (bounds.w - textWidth) / 2, 3, title.c_str());
    SDL_Rect lineRect = {3, 3+textHeight+2, bounds.w - 6, 1};
    SDL_FillRect(surface, &lineRect, SDL_MapRGB(surface->format, COL_BORDER));

    // Draw list if enabled
    if (hasListBox) {
        for (int i = scrollOffset; i < std::min(scrollOffset + visibleItems, static_cast<int>(items.size())); i++) {
            int y = listBoxRect.y + (i - scrollOffset) * listRowHeight;
            
            if (i == selectedIndex) {
                SDL_Rect highlight = {
                    listBoxRect.x, 
                    y, 
                    listBoxRect.w, 
                    listRowHeight
                };
                SDL_FillRect(surface, &highlight, SDL_MapRGB(surface->format, COL_HIGHLIGHT));
            }
            SFont_Write(surface, font, listBoxRect.x + 1, y, items[i].c_str());
        }
    }

    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
    SDL_RenderCopy(renderer, texture, NULL, &bounds);

    for (auto button : buttons) {
        button->Draw();
    }
} 