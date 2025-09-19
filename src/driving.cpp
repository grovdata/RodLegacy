#include "driving.h"
#include "state.h"
#include "parsers/libp.h"

Driving::Driving() : Scene() {
    // Create racing component
    racing = std::make_unique<Racing>();
}

void Driving::Init(bool debugMode) {
    Scene::Init(debugMode);
    
    // Load dashboard using asset system
    auto dashboard = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_INTERIOR_1});
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);
    AddDrawable("dashboard", std::shared_ptr<IDrawable>(dashboard));

    // Load pillar (windshield beam)
    auto pillar = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WINDSHIELD_BEAM});
    pillar->AlignAbove(dashboard);
    pillar->SetVisible(true);
    AddDrawable("pillar", std::shared_ptr<IDrawable>(pillar));

    // Load roof (ceiling)
    auto roof = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_CEILING});
    roof->AlignAbove(pillar);
    roof->SetVisible(true);
    AddDrawable("roof", std::shared_ptr<IDrawable>(roof));

    // Load hood
    auto hood = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD});
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    AddDrawable("hood", std::shared_ptr<IDrawable>(hood));

    // Load mirror stem
    auto mirrorStem = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_THING3});
    mirrorStem->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    mirrorStem->SetVisible(true);
    AddDrawable("mirror_stem", std::shared_ptr<IDrawable>(mirrorStem));

    // Load mirror
    auto mirror = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_BOX});
    mirror->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    mirror->SetVisible(true);
    AddDrawable("mirror", std::shared_ptr<IDrawable>(mirror));
}

Driving::~Driving() {
    SDL_RemoveTimer(timerID);
}

void Driving::Start() {
    timerID = SDL_AddTimer(250, AnimationTimerCallback, this);
}

void Driving::Draw() {
    Scene::Draw();

    roadTexture = SDL_CreateTextureFromSurface(renderer, racing->GetSurface());
    SDL_RenderCopy(renderer, roadTexture, NULL, &roadRect);

    // Draw all interior elements using drawable system
    // Order is important for rendering (back to front)
    auto roof = GetDrawable("roof");
    if (roof) roof->Draw();
    
    auto pillar = GetDrawable("pillar");
    if (pillar) pillar->Draw();
    
    auto hood = GetDrawable("hood");
    if (hood) hood->Draw();
    
    auto dashboard = GetDrawable("dashboard");
    if (dashboard) dashboard->Draw();
    
    auto mirrorStem = GetDrawable("mirror_stem");
    if (mirrorStem) mirrorStem->Draw();
    
    auto mirror = GetDrawable("mirror");
    if (mirror) mirror->Draw();
}

void Driving::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
    lastEvent = event;
}

Uint32 Driving::AnimationTimerCallback(Uint32 interval, void* param) {
    Driving* d = static_cast<Driving*>(param);
    d->racing->Draw(state.GetElapsedTime() / 1000.0f, d->lastEvent);
    //d->lastEvent = NULL;
    return interval;
}
