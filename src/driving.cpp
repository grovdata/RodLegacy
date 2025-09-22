#include "driving.h"
#include "state.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

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

    // Load hood with current car's color
    auto hood = [&]() {        
        StandardCarColor carColor = state.GetCurrentCar()->GetColor();
        std::vector<uint16_t> carPalette = GetStandardCarColor(carColor);
        byte_vector& libFile = state.assets.GetLibPData();
        return state.assets.GetBitmapAssetWithPalette(libFile, {LibAssetRegular, SrLibFileAmigaLibP, RACING_HOOD}, carPalette);    
    }();
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);
    AddDrawable("hood", std::shared_ptr<IDrawable>(hood));

    // Load mirror stem
    // auto mirrorStem = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_THING3});
    // mirrorStem->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    // mirrorStem->SetVisible(true);
    // AddDrawable("mirror_stem", std::shared_ptr<IDrawable>(mirrorStem));

    // Load mirror
    auto mirror = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_UNKNOWN_BOX});
    mirror->SetPosition(240, 19);
    mirror->SetVisible(true);
    AddDrawable("mirror", std::shared_ptr<IDrawable>(mirror));

    // Load plush dice
    auto plushDice = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_RACING_DICE1});
    plushDice->SetPosition(259, 45);
    plushDice->SetVisible(true);
    AddDrawable("plush_dice", std::shared_ptr<IDrawable>(plushDice));

    // Load wheel left hand 1
    auto wheelLeftHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_1});
    wheelLeftHand1->SetPosition(72, 144);
    wheelLeftHand1->SetVisible(true);
    AddDrawable("wheel_left_hand_1", std::shared_ptr<IDrawable>(wheelLeftHand1));

    // Load wheel right hand 1
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

    auto plushDice = GetDrawable("plush_dice");
    if (plushDice) plushDice->Draw();

    auto wheelLeftHand1 = GetDrawable("wheel_left_hand_1");
    if (wheelLeftHand1) wheelLeftHand1->Draw();
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
