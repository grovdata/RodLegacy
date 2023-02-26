#ifndef GARAGE_H
#define GARAGE_H

#include "global.h"
#include "scene.h"

class Garage: public Scene {

	public:
		Garage() : Scene("../data/rip/garage.png") {
			SDL_Rect bottleRect;
			bottleRect.x = 7; bottleRect.y = 161; bottleRect.w = 8; bottleRect.h = 24;

			Zone bottle(strdup("Wanna drink?"), &bottleRect);

			SDL_Rect newsPaperRect;
			newsPaperRect.x = 15; newsPaperRect.y = 158; newsPaperRect.w = 44; newsPaperRect.h = 25;
			Zone newsPaper(strdup("Read newspaper"), &newsPaperRect);

			m_zones.push_back(bottle);
			m_zones.push_back(newsPaper);

		}
};

#endif