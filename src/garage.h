#ifndef GARAGE_H
#define GARAGE_H

#include "global.h"
#include "scene.h"

class Garage: public Scene {

	public:
		Garage() : Scene("../data/rip/garage_bar.png") {
			SDL_Rect bottleRect = {7, 161, 8, 24};
			Zone bottle(strdup("Wanna drink ?"), &bottleRect);

			SDL_Rect newsPaperRect = {15, 158, 44, 25};
			Zone newsPaper(strdup("Check out the newspaper"), &newsPaperRect);

			SDL_Rect changeTiresRect = {136, 157, 26, 29};
			Zone changeTires(strdup("Change tires"), &changeTiresRect);

			SDL_Rect newPaintJobRect = {228, 160, 33, 24};
			Zone newPaintJob(strdup("New paint job - $20"), &newPaintJobRect);

			SDL_Rect gasRect = {288, 161, 23, 26};
			Zone gas(strdup("Wanna gas ?"), &gasRect);

			SDL_Rect carInfoRect = {290, 107, 27, 14};
			Zone carInfo(strdup("Car info"), &carInfoRect);

			SDL_Rect calendarRect = {299, 56, 16, 45};
			Zone calendar(strdup("Calendar"), &calendarRect);

			SDL_Rect radioRect = {259, 61, 25, 12};
			Zone radio(strdup("Squelsh it !"), &radioRect);

			SDL_Rect stickersRect = {216, 63, 17, 10};
			Zone stickers(strdup("Stickers - $5"), &stickersRect);

			SDL_Rect partsRect = {203, 79, 17, 19};
			Zone parts(strdup("Sell parts"), &partsRect);

			SDL_Rect menuRect = {156, 65, 33, 14};
			Zone menu(strdup("Time to quit"), &menuRect);

			SDL_Rect carsRect = {69, 41, 200, 16};
			Zone cars(strdup("Cars"), &carsRect);

			SDL_Rect streetRect = {0, 42, 40, 62};
			Zone street(strdup("Hit the street"), &streetRect);


			m_zones.push_back(bottle);
			m_zones.push_back(newsPaper);
			m_zones.push_back(changeTires);
			m_zones.push_back(newPaintJob);
			m_zones.push_back(gas);
			m_zones.push_back(carInfo);
			m_zones.push_back(calendar);
			m_zones.push_back(radio);
			m_zones.push_back(stickers);
			m_zones.push_back(parts);
			m_zones.push_back(menu);
			m_zones.push_back(cars);
			m_zones.push_back(street);
			

		}
};

#endif