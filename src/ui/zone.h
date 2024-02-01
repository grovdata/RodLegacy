#pragma once

#include "../global.h"

class Zone {
    private:
       	char *text;
       	SDL_Rect rect;
    public:
        Zone(char *text, SDL_Rect *rect) {
        	text = text;
        	rect.x = rect->x;
        	rect.y = rect->y;
        	rect.w = rect->w;
        	rect.h = rect->h;
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

