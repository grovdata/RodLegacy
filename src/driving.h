#pragma once

#include "scene.h"
#include "bitmap.h"
#include "spritesheet.h"

#include <vector>

using namespace std;

class Driving: public Scene {
   private:

    vector<Bitmap*> interior;
    
    public:
        Driving();
        ~Driving();

        // MousePointer GetPointerType() override;

        // SDL_Point GetCarIdlePosition() override;
        // SDL_Point GetCarOffscreenPosition() override;

        // void Action(ZoneId zone) override;
        void Draw() override;
};
