#ifndef SCENE_HPP
#define SCENE_HPP

#include "global.h"

class Scene {
	private:

		void SetStatusBar(char *text) {
			SFont_Write(m_FontSurface, font, 10, 300, text);
		}

		void CheckMouseHover(int x, int y) {
			for (int i = 0; i < m_zones.size(); i++) {
				Zone zone = m_zones.at(i);				
				if (zone.IsInsideZone(x, y)) {
					printf("bingo %d %d %s\n", x, y, zone.GetText());
					SetStatusBar(zone.GetText());
				}
			}		
		}

	protected:
		SDL_Surface *m_Surface, *m_FontSurface;
		SDL_Texture *m_Texture, *m_FontTexture;

		string m_backgroundName;
		vector<Zone> m_zones;
		vector<Button> m_buttons;

	public:
		Scene(string backgroundName) {
			m_Surface = IMG_Load(backgroundName.c_str());
			m_Texture = SDL_CreateTextureFromSurface(renderer, m_Surface);
			m_FontSurface = SDL_CreateRGBSurface(0, 320, 200, 32,
						0x00FF0000,
						0x0000FF00,
						0x000000FF,
						0xFF000000);
		}

		~Scene() {
			SDL_FreeSurface(m_FontSurface);
    		SDL_DestroyTexture(m_FontTexture);
    		SDL_FreeSurface(m_Surface);
    		SDL_DestroyTexture(m_Texture);
		}

		SDL_Texture *GetTexture() {
			return m_Texture;
		}

		void PushEvent(SDL_Event *event) {
			switch (event->type) {
			case SDL_MOUSEMOTION: {
				CheckMouseHover(event->motion.x, event->motion.y);					
				break;
			}
			default:
				break;
			}
		}
};

#endif
