#include "button.h"
#include "../state.h"

#define COL_GRAY      144, 144, 144
#define COL_BLACK     0, 0, 0
#define COL_WHITE     255, 255, 255
#define COL_HIGHLIGHT 0, 120, 130

Button::Button(std::string text, int offsetX, int offsetY, int width, int height) {
    this->text = text;
    bounds = {offsetX, offsetY, width, height};
    visible = true;
    pressed = false;
    
    CreateButtonSurface();
}

Button::Button(const std::string& text, const SDL_Rect& bounds, std::function<void()> onClick) 
    : text(text), bounds(bounds), onClick(onClick), pressed(false), visible(true) 
{
    CreateButtonSurface();
}

void Button::CreateButtonSurface() {
    m_Surface = SDL_CreateRGBSurface(0, bounds.w, bounds.h, 32,
                                    0x00FF0000,
                                    0x0000FF00,
                                    0x000000FF,
                                    0xFF000000);

    fontSurface = SDL_CreateRGBSurface(0, bounds.w, bounds.h, 32,
                                    0x00FF0000,
                                    0x0000FF00,
                                    0x000000FF,
                                    0xFF000000);
    
    m_Rect = bounds;
    SDL_FillRect(m_Surface, NULL, SDL_MapRGB(m_Surface->format, COL_GRAY));

    SDL_Rect border = {1, 1, bounds.w - 2, bounds.h - 2};
    SDL_FillRect(m_Surface, &border, SDL_MapRGB(m_Surface->format, COL_BLACK));

    SDL_Rect innerFill = {2, 2, bounds.w - 4, bounds.h - 4};
    SDL_FillRect(m_Surface, &innerFill, SDL_MapRGB(m_Surface->format, pressed ? COL_HIGHLIGHT : COL_GRAY));

    int textWidth = SFont_TextWidth(state.assets.GetMainFont(), text.c_str());
    int textHeight = SFont_TextHeight(state.assets.GetMainFont());
    int textX = (bounds.w - textWidth) / 2;
    int textY = (bounds.h - textHeight) / 2;

    // Draw text at centered position
    SFont_Write(fontSurface, state.assets.GetMainFont(), textX, textY, text.c_str());
    fontTexture = SDL_CreateTextureFromSurface(renderer, fontSurface);
    SDL_SetTextureColorMod(fontTexture, COL_BLACK);

    m_Texture = SDL_CreateTextureFromSurface(renderer, m_Surface);
}

Button::~Button() {
    if (m_Surface) {
        SDL_FreeSurface(m_Surface);
    }
    if (fontSurface) {
        SDL_FreeSurface(fontSurface);
    }
    if (m_Texture) {
        SDL_DestroyTexture(m_Texture);
    }
    if (fontTexture) {
        SDL_DestroyTexture(fontTexture);
    }
}

void Button::Draw() {
    if (!visible) return;
    SDL_RenderCopy(renderer, m_Texture, NULL, &m_Rect);
    SDL_RenderCopy(renderer, fontTexture, NULL, &m_Rect);
}

void Button::HandleEvent(SDL_Event* event) {
    if (!visible || !onClick) return;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button == SDL_BUTTON_LEFT) {
                int x = event->button.x;
                int y = event->button.y;
                if (x >= bounds.x && x < bounds.x + bounds.w &&
                    y >= bounds.y && y < bounds.y + bounds.h) {
                    pressed = true;
                    CreateButtonSurface(); // Update appearance
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && pressed) {
                pressed = false;
                CreateButtonSurface(); // Update appearance
                
                int x = event->button.x;
                int y = event->button.y;
                if (x >= bounds.x && x < bounds.x + bounds.w &&
                    y >= bounds.y && y < bounds.y + bounds.h) {
                    onClick();
                }
            }
            break;
    }
}

SDL_Texture* Button::GetTexture() {
    return m_Texture;
}

SDL_Rect* Button::GetRect() {
    return &m_Rect;
}