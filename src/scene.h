#pragma once

#include "global.h"
#include "ui/zone.h"
#include "ui/button.h"
#include "ui/bubble.h"
#include "mouse.h"
#include "carbitmap.h"

using namespace std;

typedef enum {
    SCENE_GARAGE,
    SCENE_DINER,
    SCENE_GAS_STATION,
    SCENE_CRUISING,
    SCENE_DRIVING
} GameScene;

typedef enum {
    SCENE_BLACK,
    SCENE_SOLID,
    SCENE_FADE_OUT,
    SCENE_FADE_IN
} SceneTransition;

class Car;

class Scene {
   private:
    ZoneId CheckMouseHover(int x, int y);
    static Uint32 FadeCallback(Uint32 interval, void* param);
    SDL_TimerID fadeTimerID;

   protected:
    SceneTransition sceneTransition;
    GameScene sceneTransitioningTo;

    SDL_Surface *surface, *fontSurface, *fadeSurface;
    SDL_Texture *texture, *fontTexture, *fadeTexture;
    SDL_Rect m_backgroundRect, m_statusBarRect, m_statusBarMiddleRect;

    string m_backgroundName;
    vector<Zone> zones;
    vector<Button> buttons;
    vector<SDL_Texture*> textures;
    vector<Bubble*> bubbles;

   public:
    Scene(string backgroundName);
    ~Scene();
    SDL_Texture *GetTexture();

    virtual MousePointer GetPointerType();

    void FadeOut();
    void FadeIn();
    void PrepareFadeIn();
    virtual void OnFadeComplete();

    virtual void Action(ZoneId zone);
    virtual void Draw();
    virtual void PushEvent(SDL_Event *event);

    virtual void SetStatusBar(char *text);
    void ClearStatusBar();

    void SetZoneText(ZoneId id, char* text);
};

class CarScene: public Scene {
    public:
        CarScene(std::string backgroundName);
        virtual void DrawMaskAfterCar();
        virtual void StartAnimateCar(Car *car, Driver *driver, bool keepDriving);
        virtual SDL_Point GetCarIdlePosition();
        virtual SDL_Point GetCarOffscreenPosition();

        CarBitmap *carBitmap;

        // Used by animation callback
        Uint64 t = 0;
        bool animatingCar = false;
        int carx, cary;
        SDL_TimerID carTimerID;
};