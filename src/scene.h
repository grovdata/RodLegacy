#pragma once

#include "global.h"
#include "ui/zone.h"
#include "ui/button.h"

typedef enum {
    SCENE_GARAGE,
    SCENE_DINER,
    SCENE_GAS_STATION
} GameScene;

class Car;

class Scene {
   private:
    ZoneId CheckMouseHover(int x, int y);

   protected:
    SDL_Surface *m_Surface, *m_FontSurface;
    SDL_Texture *m_Texture, *m_FontTexture;
    SDL_Rect m_backgroundRect, m_statusBarRect, m_statusBarMiddleRect;

    std::string m_backgroundName;
    std::vector<Zone> m_zones;
    std::vector<Button> m_buttons;


   public:
    Scene(std::string backgroundName);
    ~Scene();
    SDL_Texture *GetTexture();

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
