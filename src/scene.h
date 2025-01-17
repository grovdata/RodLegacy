#pragma once

#include "global.h"
#include "ui/zone.h"
#include "ui/button.h"
#include "ui/bubble.h"
#include "ui/hud.h"
#include "mouse.h"
#include "assets.h"
#include "bitmap.h"
#include "carbitmap.h"
#include "debug_overlay.h"
#include "parsers/libp.h"

using namespace std;

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
    bool isSetup;

    SceneTransition sceneTransition;
    SrLibGameScene sceneTransitioningTo;

    Asset backgroundAsset;
    StockPalette palette;
    Bitmap *background;
    HUD *hud;

    SDL_Surface *surface, *fontSurface, *fadeSurface;
    SDL_Texture *texture, *fontTexture, *fadeTexture;
    SDL_Rect m_backgroundRect;
    SrLibAmigaSpriteAsset currentPointer = SrLibAmigaSpriteAssetMousePointerHand;

    string backgroundName;
    vector<Zone> zones;
    vector<Button> buttons;
    vector<SDL_Texture*> textures;
    vector<Bubble*> bubbles;

    DebugOverlay* debugOverlay;
    bool debugMode;
    bool debugEnabled;
    
    Uint8 fadeAlpha = 255;
    
    virtual void ToggleDebugMode();
    virtual void DrawDebugZones();

   public:
    Scene(Asset backgroundAsset, StockPalette palette = StockPaletteAmigaDiner);
    Scene(string backgroundName);
    ~Scene();
    void Init(bool debugMode);

    SDL_Texture *GetTexture();

    virtual SrLibAmigaSpriteAsset GetPointerType() { return currentPointer; }

    void FadeOut();
    void FadeIn();
    void PrepareFadeIn();
    virtual void OnFadeComplete();

    virtual void Action(ZoneId zone);
    virtual void Draw();
    virtual void PushEvent(SDL_Event *event);

    void SetZoneText(ZoneId id, char* text);
    void SetZoneRect(ZoneId id, SDL_Rect *rect);
    Zone& GetZone(ZoneId id);
    virtual void DrawDebugUI() {}

    virtual void UpdateDebugInfo();
};

enum class CarAnimationType {
    OPPONENT_CONTINUOUS,  // For Diner: Cars drive in, wait, drive out, repeat
    SINGLE_CAR_DRIVE,    // For Gas Station: Car drives in to idle position and stops
    SINGLE_CAR_REVERSE   // For future use: Car reverses into position
};

class CarScene: public Scene {
private:
    double carAnimationTime = 0;
    double carExitAnimationTime = 0;
    double waitTimer = 0;
    bool exitingCar = false;
    const double WAIT_BEFORE_EXIT = 3.0;
    const double WAIT_BEFORE_NEXT = 2.0;
    bool isPaused = false;

protected:
    CarAnimationType currentAnimationType = CarAnimationType::SINGLE_CAR_DRIVE;
    bool initialDelay = true;
    Uint64 fadeCompleteTime = 0;
    double initStartTime;
    double lastFrameTime;
    double currentTime;

    virtual void UpdateCarAnimation(double deltaTime);
    virtual void OnCarAnimationComplete() {};
    virtual void OnCarExitComplete() {}

    bool needsNewAnimation = true;

public:
    CarScene(std::string backgroundName);
    CarScene(Asset backgroundAsset);
    virtual void Init(bool debugMode);
    virtual void DrawMaskAfterCar();
    virtual void StartAnimateCar(Car *car, Driver *driver, CarAnimationType animType);
    virtual SDL_Point GetCarIdlePosition();
    virtual void UpdateDebugInfo() override;
    virtual void PushEvent(SDL_Event *event) override;
    virtual void OnFadeComplete() override;

    CarBitmap *carBitmap;
    bool animatingCar = false;
    int carx, cary;
};
