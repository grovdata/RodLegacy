#pragma once

#include "driver.h"
#include "global.h"
#include "state.h"

class Scene {
   private:
    void CheckMouseHover(int x, int y);

   protected:
    SDL_Surface *m_Surface, *m_FontSurface;
    SDL_Texture *m_Texture, *m_FontTexture;
    SDL_Rect m_backgroundRect, m_statusBarRect, m_statusBarMiddleRect;

    string m_backgroundName;
    vector<Zone> m_zones;
    vector<Button> m_buttons;


   public:
    Scene(string backgroundName);
    ~Scene();
    SDL_Texture *GetTexture();
    virtual void Draw();
    virtual void DrawMaskAfterCar();
    virtual void PushEvent(SDL_Event *event);
    virtual SDL_Point GetCarIdlePosition();
    virtual SDL_Point GetCarOffscreenPosition();
    /// @brief Make a car pull into the scene
    /// @param car
    /// @param driver
    virtual void StartAnimateCar(CarModel *car, Driver *driver, bool keepDriving);

    void SetStatusBar(char *text);
    void ClearStatusBar();
};
