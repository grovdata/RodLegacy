#pragma once

#include "global.h"
#include "ui/zone.h"
#include "ui/button.h"
#include "ui/bubble.h"
#include "mouse.h"

typedef enum {
    SCENE_GARAGE,
    SCENE_DINER,
    SCENE_GAS_STATION
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

    std::string m_backgroundName;
    std::vector<Zone> zones;
    std::vector<Button> buttons;
    std::vector<SDL_Texture*> textures;
    std::vector<Bubble*> bubbles;

   public:
    Scene(std::string backgroundName);
    ~Scene();
    SDL_Texture *GetTexture();

    virtual MousePointer GetPointerType();

    void FadeOut();
    void FadeIn();
    void PrepareFadeIn();
    virtual void OnFadeComplete();

    virtual void Action(ZoneId zone);
    virtual void Draw();
    virtual void DrawMaskAfterCar();
    virtual void PushEvent(SDL_Event *event);
    virtual SDL_Point GetCarIdlePosition();
    virtual SDL_Point GetCarOffscreenPosition();
    /// @brief Make a car pull into the scene
    /// @param car
    /// @param driver
    virtual void StartAnimateCar(Car *car, Driver *driver, bool keepDriving);

    virtual void SetStatusBar(char *text);
    void ClearStatusBar();

    void SetZoneText(ZoneId id, char* text);
};
