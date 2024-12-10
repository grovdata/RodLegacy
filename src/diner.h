#pragma once

#include "scene.h"
#include "spritesheet.h"
#include "carbitmap.h"
#include "debug_overlay.h"
class Opponent;

class Diner: public CarScene {
   private:
    Spritesheet *phonebookGuy;
    Spritesheet *waitress;

    SDL_Surface *roadMaskSurface;
    SDL_Texture *roadMaskTexture;
    SDL_Rect roadMaskSrcRect  = {0, 155, 320, 10}; //200-45-11};
    SDL_Rect roadMaskDestRect = {0, 0, 320, 10};

    int guySpriteX;
    int guySpriteY;
    SDL_Rect guyRect = {189, 65, 38, 60};

    bool exitingCar;
    double waitTimer;

    string GetOpponentText(Opponent *opponent);

    // Add a map to store opponent car bitmaps
    std::map<std::string, std::unique_ptr<CarBitmap>> opponentCarBitmaps;
    
    // Remove the inherited carBitmap since we'll manage it differently
    // CarBitmap *carBitmap;  // Comment out or remove this line
    
    // Keep track of current active bitmap
    CarBitmap* activeCarBitmap;
    Opponent *currentOpponent;

    bool isPaused;

    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double lastFrameTime;
    double currentTime;

    public:
        Diner();
        ~Diner();
        void Init(bool debugMode) override;

        MousePointer GetPointerType() override;

        SDL_Point GetCarIdlePosition() override;
        SDL_Point GetCarOffscreenPosition() override;

        void Action(ZoneId zone) override;

        void Draw() override;
        void DrawMaskAfterCar() override;
        // void SetStatusBar(char *text) override;

        void StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType) override;
        void UpdateAnimation(double deltaTime);


        void UpdateDebugInfo() override;

        void PushEvent(SDL_Event *event) override;

        void OnFadeComplete() override;
};
