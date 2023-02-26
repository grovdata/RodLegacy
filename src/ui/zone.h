#ifndef ZONE_H
#define ZONE_H

#include "../global.h"

class Zone {
    private:
       	char *m_text;
       	SDL_Rect m_rect;
    public:
        Zone(char *text, SDL_Rect *rect) {
        	m_text = text;
        	m_rect.x = rect->x;
        	m_rect.y = rect->y;
        	m_rect.w = rect->w;
        	m_rect.h = rect->h;
        }
        ~Zone() {

        }

        bool IsInsideZone(int x, int y) {
        	//printf("Zone %s %d %d %d %d\n", m_text, m_rect.x, m_rect.y, m_rect.x + m_rect.w, m_rect.x + m_rect.h);
        	return (x >= m_rect.x) && (x <= (m_rect.x + m_rect.w)) && (y >= m_rect.y) && (y <= (m_rect.y + m_rect.h));
        }

        char* GetText() {
        	return m_text;
        }

};

#endif