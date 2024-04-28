#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"
#include "racing.h"

#include <vector>

using namespace std;

class Driving: public Scene {
   private:

    vector<Bitmap*> interior;
    Racing *racing;

    SDL_Event *lastEvent;
    SDL_Rect roadRect = {0, 14, GAME_WIDTH, GAME_RACING_HEIGHT};
    SDL_Texture *roadTexture;

    public:
        Driving();
        ~Driving();

        // MousePointer GetPointerType() override;

        // SDL_Point GetCarIdlePosition() override;
        // SDL_Point GetCarOffscreenPosition() override;

        // void Action(ZoneId zone) override;

        void Draw() override;
        void PushEvent(SDL_Event *event) override;
};
