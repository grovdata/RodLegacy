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
    ZONE_GARAGE_REAR_BUMPER,
    ZONE_GARAGE_CHOPPED_ROOF,
    ZONE_GARAGE_FRONT_BUMPER,
    ZONE_GARAGE_TRANSMISSION,
    ZONE_GARAGE_ENGINE,
    ZONE_GARAGE_BUTTON_SWITCH_IT,
    ZONE_GARAGE_BUTTON_SELL_IT,
    ZONE_GARAGE_BUTTON_FORGET_IT,
    ZONE_GARAGE_BUTTON_SCROLL_TOP,
    ZONE_GARAGE_BUTTON_SCROLL_UP,
    ZONE_GARAGE_BUTTON_SCROLL_DOWN,
    ZONE_GARAGE_BUTTON_SCROLL_BOTTOM,
    ZONE_GARAGE_MENU_SAVE_GAME,
    ZONE_GARAGE_MENU_OLD_GAME,
    ZONE_GARAGE_MENU_NEW_GAME,
    ZONE_GARAGE_MENU_QUIT_GAME,
    ZONE_GARAGE_MENU_FORGET_IT,
    ZONE_GARAGE_ENGINE_OVERVIEW_TUNE,
    ZONE_GARAGE_ENGINE_OVERVIEW_PARTS,
    ZONE_GARAGE_ENGINE_OVERVIEW_DONE,
    ZONE_GARAGE_ENGINE_OVERVIEW_RETARD,
    ZONE_GARAGE_ENGINE_OVERVIEW_ADVANCE,
    ZONE_GARAGE_ENGINE_TUNE_DONE,
    ZONE_MAGAZINE_AUTO_PARTS,
    ZONE_MAGAZINE_USED_CARS,
    ZONE_MAGAZINE_BACK_TO_GARAGE,
    ZONE_MAGAZINE_PREVIOUS,
    ZONE_MAGAZINE_NEXT,
    ZONE_MAGAZINE_BACK_TO_GARAGE2,
    ZONE_DINER_CALL,
    ZONE_DINER_GOTO_GARAGE,
    ZONE_DINER_ORDER,
    ZONE_DINER_BACKGROUND,
    ZONE_DINER_CHALLENGE_OPPONENT,
    ZONE_DINER_POP_HOOD,
    ZONE_GAS_STATION_PUMP,
    ZONE_GAS_STATION_GAS_CAP,
    ZONE_GAS_STATION_RAG,
    ZONE_GAS_STATION_CAR_WINDOW_REAR,
    ZONE_GAS_STATION_CAR_WINDOW_FRONT,
    ZONE_GAS_STATION_GUS,
} ZoneId;

class Zone {
    private:
        ZoneId id;
       	char *text;
       	SDL_Rect rect;
        bool enabled;
    public:
        Zone(ZoneId id, char *text, SDL_Rect *rect, bool enabled = true) {
            this->id = id;
        	this->text = text;
        	this->rect.x = rect->x;
        	this->rect.y = rect->y;
        	this->rect.w = rect->w;
        	this->rect.h = rect->h;
            this->enabled = enabled;
        }
        ~Zone() {

        }

        bool IsInsideZone(int x, int y) {
            if (!enabled) return false;
        	//printf("Zone %s %d %d %d %d\n", m_text, m_rect.x, m_rect.y, m_rect.x + m_rect.w, m_rect.x + m_rect.h);
        	return (x >= rect.x) && (x <= (rect.x + rect.w)) && (y >= rect.y) && (y <= (rect.y + rect.h));
        }

        char* GetText() {
        	return text;
        }

        void SetText(char *text) {
            this->text = text;
        }

        void SetEnabled(bool enabled) {
            this->enabled = enabled;
        }

        const SDL_Rect GetRect() {
            return rect;
        }

        void SetRect(SDL_Rect *rect) {
            this->rect.x = rect->x;
            this->rect.y = rect->y;
            this->rect.w = rect->w;
            this->rect.h = rect->h;
        }

        ZoneId GetId() {
            return id;
        }

};

