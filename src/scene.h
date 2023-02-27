#ifndef SCENE_HPP
#define SCENE_HPP

#include "global.h"

class Scene {
	private:

		void SetStatusBar(char *text) {
			SFont_Write(m_FontSurface, font, 161, 200-9, text);
			m_FontTexture = SDL_CreateTextureFromSurface(renderer, m_FontSurface);
			SDL_SetTextureColorMod(m_FontTexture, 119, 102, 17);
		}

		void ClearStatusBar() {
			SDL_FillRect(m_FontSurface, &m_statusBarRect, 0);
			m_FontTexture = SDL_CreateTextureFromSurface(renderer, m_FontSurface);
		}

		void CheckMouseHover(int x, int y) {
			for (int i = 0; i < m_zones.size(); i++) {
				Zone zone = m_zones.at(i);
				if (zone.IsInsideZone(x, y)) {
					//printf("bingo %d %d %s\n", x, y, zone.GetText());
					ClearStatusBar();
					SetStatusBar(zone.GetText());
					return;
				}
			}
			// If no hover, clear bar
			ClearStatusBar();
		}

	protected:
		SDL_Surface *m_Surface, *m_FontSurface;
		SDL_Texture *m_Texture, *m_FontTexture;
		SDL_Rect m_backgroundRect, m_statusBarRect;

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
			m_backgroundRect.x = 0;
			m_backgroundRect.y = 0;
			m_backgroundRect.w = 320;
			m_backgroundRect.h = 200;

			m_statusBarRect.x = 0;
			m_statusBarRect.y = 200-9;
			m_statusBarRect.w = 320;
			m_statusBarRect.h = 9;
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

		void Draw() {
			SDL_RenderCopy(renderer, m_Texture, NULL, &m_backgroundRect);
			SDL_RenderCopy(renderer, m_FontTexture, NULL, &m_backgroundRect);
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
