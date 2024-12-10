#pragma once

#include "../global.h"

typedef enum {
    ZONE_NULL = 0,
    ZONE_GARAGE_NEWSPAPER,
    ZONE_GARAGE_TIRES,
    ZONE_GARAGE_DRINK,
    ZONE_GARAGE_PAINT_JOB,
    ZONE_GARAGE_GAS_CAN,
    ZONE_GARAGE_CAR_INFO,
    ZONE_GARAGE_CALENDAR,
    ZONE_GARAGE_MUSIC,
    ZONE_GARAGE_STICKERS,
    ZONE_GARAGE_SELL_PARTS,
    ZONE_GARAGE_MENU,
    ZONE_GARAGE_CAR_LIST,
    ZONE_GARAGE_GOTO_DINER,
    ZONE_DINER_CALL,
    ZONE_DINER_GOTO_GARAGE,
    ZONE_DINER_ORDER,
    ZONE_DINER_BACKGROUND,
    ZONE_DINER_CHALLENGE_OPPONENT,
    ZONE_DINER_POP_HOOD,
    ZONE_GAS_STATION_PUMP,
    ZONE_GAS_STATION_CAR_WINDOW,
    ZONE_GAS_STATION_GUS,
} ZoneId;

class Zone {
    private:
        ZoneId id;
       	char *text;
       	SDL_Rect rect;
    public:
        Zone(ZoneId id, char *text, SDL_Rect *rect) {
            this->id = id;
        	this->text = text;
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

        void SetText(char *text) {
            this->text = text;
        }

        ZoneId GetId() {
            return id;
        }

};

