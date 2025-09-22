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
    wheelLeftHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_1});
    wheelLeftHand1->SetPosition(72, 144);
    wheelLeftHand1->SetVisible(true);
    AddWheelDrawable("wheel_left_hand_1", wheelLeftHand1);

    wheelLeftHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_2});
    wheelLeftHand2->SetPosition(72, 120);
    wheelLeftHand2->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_2", wheelLeftHand2);

    wheelLeftHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_3});
    wheelLeftHand3->SetPosition(24, 144);
    wheelLeftHand3->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_3", wheelLeftHand3);

    wheelLeftHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_LEFT_HAND_4});
    wheelLeftHand4->SetPosition(72, 144);
    wheelLeftHand4->SetVisible(false);
    AddWheelDrawable("wheel_left_hand_4", wheelLeftHand4);

    // Load wheel right hand 1
    wheelRightHand1 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_1});
    wheelRightHand1->SetPosition(216, 144);
    wheelRightHand1->SetVisible(true);
    AddWheelDrawable("wheel_right_hand_1", wheelRightHand1);

    // Load wheel right hand 2
    wheelRightHand2 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_2});
    wheelRightHand2->SetPosition(216, 120);
    wheelRightHand2->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_2", wheelRightHand2);

    // Load wheel right hand 3
    wheelRightHand3 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_3});
    wheelRightHand3->SetPosition(216, 144);
    wheelRightHand3->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_3", wheelRightHand3);

    // Load wheel right hand 4
    wheelRightHand4 = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_RIGHT_HAND_4});
    wheelRightHand4->SetPosition(272, 144);
    wheelRightHand4->SetVisible(false);
    AddWheelDrawable("wheel_right_hand_4", wheelRightHand4);

    wheelTurningLeft = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT});
    wheelTurningLeft->SetPosition(120, 144);
    wheelTurningLeft->SetVisible(false);
    AddWheelDrawable("wheel_turning_left", wheelTurningLeft);

    wheelTurningRight = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT});
    wheelTurningRight->SetPosition(120, 144);
    wheelTurningRight->SetVisible(false);
    AddWheelDrawable("wheel_turning_right", wheelTurningRight);

    wheelTurningLeftOpposite = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_LEFT_OPPOSITE_1});
    wheelTurningLeftOpposite->SetPosition(72, 144);
    wheelTurningLeftOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_left_opposite", wheelTurningLeftOpposite);

    wheelTurningRightOpposite = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, RACING_WHEEL_CENTER_TURNING_RIGHT_OPPOSITE_1});
    wheelTurningRightOpposite->SetPosition(216, 144);
    wheelTurningRightOpposite->SetVisible(false);
    AddWheelDrawable("wheel_turning_right_opposite", wheelTurningRightOpposite);

    SetState(DRIVING_STATE_NORMAL);
}

Driving::~Driving() {
    SDL_RemoveTimer(timerID);
}

void Driving::AddWheelDrawable(const std::string& id, Bitmap* bitmap) {
    AddDrawable(id, std::shared_ptr<IDrawable>(bitmap));
}

void Driving::Start() {
    timerID = SDL_AddTimer(250, AnimationTimerCallback, this);
}

void Driving::SetState(DrivingState state) {
    // Hide all wheel elements first
    wheelLeftHand1->SetVisible(false);
    wheelLeftHand2->SetVisible(false);
    wheelLeftHand3->SetVisible(false);
    wheelLeftHand4->SetVisible(false);
    wheelRightHand1->SetVisible(false);
    wheelRightHand2->SetVisible(false);
    wheelRightHand3->SetVisible(false);
    wheelRightHand4->SetVisible(false);
    wheelTurningLeft->SetVisible(false);
    wheelTurningRight->SetVisible(false);
    wheelTurningLeftOpposite->SetVisible(false);
    wheelTurningRightOpposite->SetVisible(false);

    this->currentState = state;
    switch (state) {
        case DRIVING_STATE_NORMAL:
            // Show default hand positions
            wheelLeftHand1->SetVisible(true);
            wheelRightHand1->SetVisible(true);
            break;
        case DRIVING_STATE_SHIFTING:
            break;
        case DRIVING_STATE_TURNING_LEFT:
            wheelRightHand2->SetVisible(true);
            wheelRightHand3->SetVisible(true);
            wheelRightHand4->SetVisible(true);
            wheelTurningLeft->SetVisible(true);
            wheelTurningLeftOpposite->SetVisible(true);
            break;
        case DRIVING_STATE_TURNING_RIGHT:
            wheelLeftHand2->SetVisible(true);
            wheelLeftHand3->SetVisible(true);
            wheelLeftHand4->SetVisible(true);
            wheelTurningRight->SetVisible(true);
            wheelTurningRightOpposite->SetVisible(true);
            break;
    }
}

void Driving::Draw() {
    Scene::Draw();

    roadTexture = SDL_CreateTextureFromSurface(renderer, racing->GetSurface());
    SDL_RenderCopy(renderer, roadTexture, NULL, &roadRect);

    // Draw all interior elements using drawable system
    DrawAll();
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
