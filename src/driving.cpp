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

    auto wheelLeftHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_2});
    wheelLeftHand2->SetPosition(72, 120);
    wheelLeftHand2->SetVisible(false);
    AddDrawable("wheel_left_hand_2", std::shared_ptr<IDrawable>(wheelLeftHand2));

    auto wheelLeftHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_3});
    wheelLeftHand3->SetPosition(24, 144);
    wheelLeftHand3->SetVisible(false);
    AddDrawable("wheel_left_hand_3", std::shared_ptr<IDrawable>(wheelLeftHand3));

    auto wheelLeftHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_4});
    wheelLeftHand4->SetPosition(72, 144);
    wheelLeftHand4->SetVisible(false);
    AddDrawable("wheel_left_hand_4", std::shared_ptr<IDrawable>(wheelLeftHand4));

    // Load wheel right hand 1
    auto wheelRightHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_1});
    wheelRightHand1->SetPosition(216, 144);
    wheelRightHand1->SetVisible(true);
    AddDrawable("wheel_right_hand_1", std::shared_ptr<IDrawable>(wheelRightHand1));

    // Load wheel right hand 2
    auto wheelRightHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_2});
    wheelRightHand2->SetPosition(216, 120);
    wheelRightHand2->SetVisible(false);
    AddDrawable("wheel_right_hand_2", std::shared_ptr<IDrawable>(wheelRightHand2));

    // Load wheel right hand 3
    auto wheelRightHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_3});
    wheelRightHand3->SetPosition(216, 144);
    wheelRightHand3->SetVisible(false);
    AddDrawable("wheel_right_hand_3", std::shared_ptr<IDrawable>(wheelRightHand3));

    // Load wheel right hand 4
    auto wheelRightHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_4});
    wheelRightHand4->SetPosition(272, 144);
    wheelRightHand4->SetVisible(false);
    AddDrawable("wheel_right_hand_4", std::shared_ptr<IDrawable>(wheelRightHand4));

    SetState(DRIVING_STATE_NORMAL);
}

Driving::~Driving() {
    SDL_RemoveTimer(timerID);
}

void Driving::Start() {
    timerID = SDL_AddTimer(250, AnimationTimerCallback, this);
}

void Driving::SetState(DrivingState state) {
    auto leftHand = GetDrawable("wheel_left_hand_1");
    if (leftHand) leftHand->SetVisible(false);
    auto leftHand2 = GetDrawable("wheel_left_hand_2");
    if (leftHand2) leftHand2->SetVisible(false);
    auto leftHand3 = GetDrawable("wheel_left_hand_3");
    if (leftHand3) leftHand3->SetVisible(false);
    auto leftHand4 = GetDrawable("wheel_left_hand_4");
    if (leftHand4) leftHand4->SetVisible(false);
    auto rightHand = GetDrawable("wheel_right_hand_1");
    if (rightHand) rightHand->SetVisible(false);
    auto rightHand2 = GetDrawable("wheel_right_hand_2");
    if (rightHand2) rightHand2->SetVisible(false);
    auto rightHand3 = GetDrawable("wheel_right_hand_3");
    if (rightHand3) rightHand3->SetVisible(false);
    auto rightHand4 = GetDrawable("wheel_right_hand_4");
    if (rightHand4) rightHand4->SetVisible(false);

    this->currentState = state;
    switch (state) {
        case DRIVING_STATE_NORMAL:
            // Show default hand positions
            leftHand->SetVisible(true);
            rightHand->SetVisible(true);
            break;
        case DRIVING_STATE_SHIFTING:
            break;
        case DRIVING_STATE_TURNING_LEFT:
            rightHand2->SetVisible(true);
            rightHand3->SetVisible(true);
            rightHand4->SetVisible(true);
            break;
        case DRIVING_STATE_TURNING_RIGHT:
            leftHand2->SetVisible(true);
            leftHand3->SetVisible(true);
            leftHand4->SetVisible(true);
            break;
    }
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

    auto wheelLeftHand2 = GetDrawable("wheel_left_hand_2");
    if (wheelLeftHand2) wheelLeftHand2->Draw();

    auto wheelLeftHand3 = GetDrawable("wheel_left_hand_3");
    if (wheelLeftHand3) wheelLeftHand3->Draw();

    auto wheelLeftHand4 = GetDrawable("wheel_left_hand_4");
    if (wheelLeftHand4) wheelLeftHand4->Draw();

    auto wheelRightHand1 = GetDrawable("wheel_right_hand_1");
    if (wheelRightHand1) wheelRightHand1->Draw();

    auto wheelRightHand2 = GetDrawable("wheel_right_hand_2");
    if (wheelRightHand2) wheelRightHand2->Draw();

    auto wheelRightHand3 = GetDrawable("wheel_right_hand_3");
    if (wheelRightHand3) wheelRightHand3->Draw();

    auto wheelRightHand4 = GetDrawable("wheel_right_hand_4");
    if (wheelRightHand4) wheelRightHand4->Draw();
}

void Driving::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
    lastEvent = event;
    
    // Handle keyboard input for steering
    if (event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
                SetState(DRIVING_STATE_TURNING_LEFT);
                break;
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_TURNING_RIGHT);
                break;
        }
    } else if (event->type == SDL_KEYUP) {
        switch (event->key.keysym.sym) {
            case SDLK_LEFT:
            case SDLK_RIGHT:
                SetState(DRIVING_STATE_NORMAL);
                break;
        }
    }
}

Uint32 Driving::AnimationTimerCallback(Uint32 interval, void* param) {
    Driving* d = static_cast<Driving*>(param);
    d->racing->Draw(state.GetElapsedTime() / 1000.0f, d->lastEvent);
    //d->lastEvent = NULL;
    return interval;
}
