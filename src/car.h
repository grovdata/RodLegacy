#pragma once

#include "global.h"
extern SDL_Renderer *renderer;
enum CarVendor {
    CHRYSLER,
    FORD,
    GM
};

struct CarModel {
    CarVendor vendor;
    std::string name;
    std::string filename;
    int year;
    int baseSpeed;
    int width, height;
};

enum CarWheelType {
    STOCK,
    BRAND,
    RACING
};

class Car {

    /*
        Car positions
         Garage: 42, 82
         Diner : 
         Gas station:
    */

    private:
        bool m_frontBumperChopped;
        bool m_backBumperChopped;
        bool m_roofChopped;
        int m_tuning;
        
        CarModel m_model;              
        CarWheelType m_tires;
        SDL_Surface *m_Surface;
        SDL_Texture *m_Texture;
        SDL_Rect m_rect;

	public:
		Car(CarModel model) : 
            m_frontBumperChopped(false),            
            m_backBumperChopped(false),
            m_roofChopped(false),
            m_model(model),
            m_tires(STOCK)
        {            
			m_Surface = IMG_Load(model.filename.c_str());
			m_Texture = SDL_CreateTextureFromSurface(renderer, m_Surface);
			m_rect.x = 0;
			m_rect.y = 0;
			m_rect.w = model.width;
			m_rect.h = model.height;
		}

		~Car() {
	 		SDL_FreeSurface(m_Surface);
    		SDL_DestroyTexture(m_Texture);
		}

		SDL_Texture *GetTexture() {
			return m_Texture;
		}

		void Draw() {
            SDL_RenderCopy(renderer, m_Texture, NULL, &m_rect);
		}

        void Move(SDL_Point p) {
            m_rect.x = p.x;
            m_rect.y = p.y;
        }

        void MoveLeft() {
            m_rect.x--;          
        }

        void MoveRight() {
            m_rect.x++;          
        }
};