#pragma once

#include "../global.h"

class Zone {
    private:
       	char *text;
       	SDL_Rect rect;
    public:
        Zone(char *text, SDL_Rect *rect) {
        	text = text;
        	this->rect.x = rect->x;
        	this->rect.y = rect->y;
        	this->rect.w = rect->w;
        	this->rect.h = rect->h;
        }
        ~Zone() {

        }

        bool IsInsideZone(int x, int y) {
        	//printf("Zone %s %d %d %d %d\n", m_text, m_rect.x, m_rect.y, m_rect.x + m_rect.w, m_rect.x + m_rect.h);
        	return (x >= rect.x) && (x <= (rect.x + rect.w)) && (y >= rect.y) && (y <= (rect.y + rect.h));
        }

        char* GetText() {
        	return text;
        }

};

