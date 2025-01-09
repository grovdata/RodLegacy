#include "garage.h"
#include "state.h"
#include "assets.h"
#include "state.h"

Garage::Garage() : CarScene((Asset){LibAssetRegular, SrLibFileAmigaLibP, GARAGE_SCREEN}) {

   SDL_Rect bottleRect = {7, 161, 8, 24};
    Zone bottle(ZONE_GARAGE_DRINK, strdup("Wanna drink ?"), &bottleRect);

    SDL_Rect newsPaperRect = {15, 158, 44, 25};
    Zone newsPaper(ZONE_GARAGE_NEWSPAPER, strdup("Check out the newspaper"), &newsPaperRect);

    SDL_Rect changeTiresRect = {136, 157, 26, 29};
    Zone changeTires(ZONE_GARAGE_TIRES, strdup("Change tires"), &changeTiresRect);

    SDL_Rect newPaintJobRect = {228, 160, 33, 24};
    Zone newPaintJob(ZONE_GARAGE_PAINT_JOB, strdup("New paint job - $20"), &newPaintJobRect);

    SDL_Rect gasRect = {288, 161, 23, 26};
    Zone gas(ZONE_GARAGE_GAS_CAN, strdup("Wanna tank ?"), &gasRect);

    SDL_Rect carInfoRect = {290, 107, 27, 14};
    Zone carInfo(ZONE_GARAGE_CAR_INFO, strdup("Car info"), &carInfoRect);

    SDL_Rect calendarRect = {299, 56, 16, 45};
    Zone calendar(ZONE_GARAGE_CALENDAR, strdup("Calendar"), &calendarRect);

    SDL_Rect radioRect = {259, 61, 25, 12};
    Zone radio(ZONE_GARAGE_MUSIC, strdup("Squelsh it !"), &radioRect);

    SDL_Rect stickersRect = {216, 63, 17, 10};
    Zone stickers(ZONE_GARAGE_STICKERS, strdup("Stickers - $5"), &stickersRect);

    SDL_Rect partsRect = {203, 79, 17, 19};
    Zone parts(ZONE_GARAGE_SELL_PARTS, strdup("Sell parts"), &partsRect);

    SDL_Rect menuRect = {156, 65, 33, 14};
    Zone menu(ZONE_GARAGE_MENU, strdup("Time to quit"), &menuRect);

    SDL_Rect carsRect = {69, 41, 200, 16};
    Zone cars(ZONE_GARAGE_CAR_LIST, strdup("Cars"), &carsRect);

    SDL_Rect streetRect = {0, 42, 40, 62};
    Zone street(ZONE_GARAGE_GOTO_DINER, strdup("Hit the street"), &streetRect);

    SDL_Rect rearBumperRect = {290, 107, 27, 14};
    Zone rearBumper(ZONE_GARAGE_REAR_BUMPER, strdup("Rear bumper"), &rearBumperRect);

    SDL_Rect frontBumperRect = {290, 107, 27, 14};
    Zone frontBumper(ZONE_GARAGE_FRONT_BUMPER, strdup("Front bumper"), &frontBumperRect);

    SDL_Rect transmissionRect = {290, 107, 27, 14};
    Zone transmission(ZONE_GARAGE_TRANSMISSION, strdup("Transmission"), &transmissionRect);

    SDL_Rect engineRect = {290, 107, 27, 14};
    Zone engine(ZONE_GARAGE_ENGINE, strdup("Pop the hood"), &engineRect);



    zones.push_back(bottle);
    zones.push_back(newsPaper);
    zones.push_back(changeTires);
    zones.push_back(newPaintJob);
    zones.push_back(gas);
    zones.push_back(carInfo);
    zones.push_back(calendar);
    zones.push_back(radio);
    zones.push_back(stickers);
    zones.push_back(parts);
    zones.push_back(menu);
    zones.push_back(cars);
    zones.push_back(street);
    zones.push_back(rearBumper);
    zones.push_back(frontBumper);
    zones.push_back(transmission);
    zones.push_back(engine);

    Bubble *bubbleDrink = new Bubble("  Don\'t drink and drive, bub!", BUBBLE_LEFT, 103, 30, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(bubbleDrink);
    Bubble *partsBubble = new Bubble("  You\'ve got no spare parts, dummy!", BUBBLE_LEFT, 93, 30, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(partsBubble);
    Bubble *tiresBubble = new Bubble("  You\'ve got no spare tires, dummy!", BUBBLE_LEFT, 79, 30, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(tiresBubble);

    currentPointer = PointerWrench;

    signSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);

    std::vector<std::string> labels;
    for (const auto& car : state.GetOwnedCars()) {
        labels.push_back(car->GetModel().name);
    }
    
    SDL_Rect bounds = {36, 26, 260, 136};

    const int buttonWidth = 66;
    const int buttonHeight = 16;
    const int buttonY = bounds.y + bounds.h - buttonHeight - 8;
    
    SDL_Rect listBoxRect = {40, 40, 242, 80};    
    SDL_Rect topRect = {37, 27, 246, 95};
    SDL_Rect bottomRect = {37, 123, 246, 40};
    SDL_Rect selectedItemRect = {39, 127, 242, 12};
    SDL_Rect switchItRect = {bounds.x + 14, buttonY, buttonWidth, buttonHeight};
    SDL_Rect sellItRect = {bounds.x + 90, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetItRect = {bounds.x + 165, buttonY, buttonWidth, buttonHeight};
    
    // Dialog zones
    Zone switchItZone(ZONE_GARAGE_BUTTON_SWITCH_IT, strdup("Switch it"), &switchItRect);
    Zone sellItZone(ZONE_GARAGE_BUTTON_SELL_IT, strdup("Sell it"), &sellItRect);
    Zone forgetItZone(ZONE_GARAGE_BUTTON_FORGET_IT, strdup("Forget it"), &forgetItRect);
    Zone scrollTopZone(ZONE_GARAGE_BUTTON_SCROLL_TOP, strdup("/"), &topRect);
    Zone scrollDownZone(ZONE_GARAGE_BUTTON_SCROLL_DOWN, strdup(">"), &topRect);
    Zone scrollUpZone(ZONE_GARAGE_BUTTON_SCROLL_UP, strdup("<"), &topRect);
    Zone scrollBottomZone(ZONE_GARAGE_BUTTON_SCROLL_BOTTOM, strdup("\\"), &topRect);

    vector<ButtonConfig> buttons = {
        {
            switchItRect,
            "Switch it",
            nullptr,  // onClick will be set by Dialog
            true,     // defaultChoice
            0        // index
        },
        {
            sellItRect,
            "Sell it",
            nullptr,
            false,
            1
        },
        {
            forgetItRect,
            "Forget it",
            nullptr,
            false,
            2
        }
    };

    vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{39, 30, 240, 10}, "Your cars", {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };

    vector<SDL_Rect> lines = {
        {40, 39, 240, 1},
    };

    // Add dialog zones to config
    vector<Zone> dialogZones = {
        switchItZone,
        sellItZone,
        forgetItZone,
        scrollTopZone,
        scrollDownZone,
        scrollUpZone,
        scrollBottomZone
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = bounds;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.buttons = buttons;
    dialogConfig.lines = lines;
    dialogConfig.zones = dialogZones;  // Add the dialog zones here
    dialogConfig.contentAreas = vector<SDL_Rect>{topRect, bottomRect};
    dialogConfig.font = state.assets.GetMainFont();
    dialogConfig.onZoneHover = [this](Zone& zone) {
        SDL_Log("Garage::OnZoneHover: zoneId=%d", zone.GetId());
        this->hud->SetText(zone.GetText());
    };
    dialogConfig.onButtonPressed = [this](int buttonIndex) {
        SDL_Log("Garage::OnButtonPressed: buttonIndex=%d", buttonIndex);
        switch (buttonIndex) {
            case 0: { // Switch it
                int selectedIndex = carSelectDialog->GetSelectedIndex();
                if (selectedIndex >= 0) {
                    Car* selectedCar = state.GetOwnedCars()[selectedIndex];
                    if (selectedCar != state.GetCurrentCar()) {
                        pendingCar = selectedCar;
                        StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                    }
                }
                carSelectDialog->Hide();
                break;
            }
            case 1: // Sell it
                carSelectDialog->Hide();
                break;
            case 2: // Forget it
                carSelectDialog->Hide();
                break;
        }
    };
    dialogConfig.texts = texts;

    ListDialogConfig listDialogConfig;
    listDialogConfig.labels = labels;
    listDialogConfig.listBoxRect = listBoxRect;
    listDialogConfig.selectedItemRect = selectedItemRect;

    carSelectDialog = std::make_unique<ListDialog>(
        dialogConfig,
        ListDialogConfig{
            labels,
            [this](int selectedIndex) {
                Car* selectedCar = state.GetOwnedCars()[selectedIndex];
            SDL_Log("Garage::ListDialog: Selected index=%d, car=%s (ptr: %p)", 
                   selectedIndex, 
                   selectedCar->GetModel().name.c_str(), 
                   (void*)selectedCar);
            
            // Only switch if selecting a different car
            if (selectedCar != state.GetCurrentCar()) {
                pendingCar = selectedCar;
                StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_REVERSE);
                SDL_Log("Garage::ListDialog: Switching from current car (ptr: %p) to pending car (ptr: %p)",
                       (void*)state.GetCurrentCar(),
                       (void*)pendingCar);
            } else {
                SDL_Log("Garage::ListDialog: Same car selected, ignoring");
            }
        },
        listBoxRect,
        selectedItemRect,
        {0xFF, 0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0x00, 0xFF},
        {
            {topRect.x + topRect.w + 2, topRect.y + 14, 15, 15},     // top
            {topRect.x + topRect.w + 2, topRect.y + 30, 15, 15},     // up
            {topRect.x + topRect.w + 2, topRect.y + topRect.h - 31, 15, 15}, // down
            {topRect.x + topRect.w + 2, topRect.y + topRect.h - 15, 15, 15}  // bottom
        }
    });

    

    InitCarBitmaps();
    
    // Set initial car bitmap
    if (state.GetCurrentCar()) {
        carBitmap = GetOrCreateCarBitmap(state.GetCurrentCar());
    }
}

SDL_Point Garage::GetCarIdlePosition() {
    int x = SrLibQueryGetCarIdleX(SCENE_GARAGE);
    int y = SrLibQueryGetCarRoofYOffset(SCENE_GARAGE, state.GetCurrentCar()->GetModel().index);
    return {x, y};
}

void Garage::UpdateSign() {
    char text[50];
    CarModel m = state.GetCurrentCar()->GetModel();
    snprintf(text, 50, "%s.", m.name.c_str());
    SFont_Write(signSurface, state.assets.GetMainFont(), 78, 45, text);
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
    SDL_RenderCopy(renderer, signTexture, NULL, &m_backgroundRect);
}

void Garage::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    switch (zone)
    {
    case ZONE_GARAGE_GOTO_DINER:
        state.MoveToScene(SCENE_DINER);
        break;
    case ZONE_GARAGE_GAS_CAN:
        state.MoveToScene(SCENE_GAS_STATION);
        break;
    case ZONE_GARAGE_DRINK:
        bubbles[0]->SetVisible(true);
        break;
    case ZONE_GARAGE_SELL_PARTS:
        bubbles[1]->SetVisible(true);
        break;
    case ZONE_GARAGE_TIRES:
        bubbles[2]->SetVisible(true);
        break;
    case ZONE_GARAGE_CAR_LIST:
        carSelectDialog->Show();
        break;
    default:
        break;
    }
}

void Garage::Draw() {
    currentTime = SDL_GetTicks64() / 1000.0;
    double deltaTime = currentTime - lastFrameTime;
    lastFrameTime = currentTime;

    UpdateCarAnimation(deltaTime);

    Scene::Draw();

    // Handle initial delay
    if (initialDelay) {
        if (SDL_GetTicks64() - fadeCompleteTime >= 2000) {
            initialDelay = false;
            
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
        return;
    }

    if (carBitmap != nullptr) {
        SDL_Point p;
        p.x = carx; p.y = cary;
        carBitmap->Move(p);
        carBitmap->Draw();

        // get clickboxes
        CarMetadata_t metadata = state.carData.GetCarMetadata(state.GetCurrentCar()->GetModel().index);
        CarClickBoxes_t clickBoxes = metadata.clickBoxes;
        
        SDL_Rect rearBumperRect = {carx + clickBoxes.clickBoxRearBumper.x, cary + clickBoxes.clickBoxRearBumper.y, clickBoxes.clickBoxRearBumper.width, clickBoxes.clickBoxRearBumper.height};
        SetZoneRect(ZONE_GARAGE_REAR_BUMPER, &rearBumperRect);

        SDL_Rect choppedRoofRect = {carx + clickBoxes.clickBoxChoppedRoof.x, cary + clickBoxes.clickBoxChoppedRoof.y, clickBoxes.clickBoxChoppedRoof.width, clickBoxes.clickBoxChoppedRoof.height};
        SetZoneRect(ZONE_GARAGE_CHOPPED_ROOF, &choppedRoofRect);

        SDL_Rect frontBumperRect = {carx + clickBoxes.clickBoxFrontBumper.x, cary + clickBoxes.clickBoxFrontBumper.y, clickBoxes.clickBoxFrontBumper.width, clickBoxes.clickBoxFrontBumper.height};
        SetZoneRect(ZONE_GARAGE_FRONT_BUMPER, &frontBumperRect);

        SDL_Rect transmissionRect = {carx + clickBoxes.clickBoxTransmission.x, cary + clickBoxes.clickBoxTransmission.y, clickBoxes.clickBoxTransmission.width, clickBoxes.clickBoxTransmission.height};
        SetZoneRect(ZONE_GARAGE_TRANSMISSION, &transmissionRect);

        SDL_Rect engineRect = {carx + clickBoxes.clickBoxEngine.x, cary + clickBoxes.clickBoxEngine.y, clickBoxes.clickBoxEngine.width, clickBoxes.clickBoxEngine.height};
        SetZoneRect(ZONE_GARAGE_ENGINE, &engineRect);
    }
   
    

    for (auto b : bubbles) {
        b->Draw();
    }

    DrawDebugZones();

    if (carSelectDialog && carSelectDialog->IsVisible()) {
        carSelectDialog->Draw();
    }
}

void Garage::OnFadeComplete() {
    if (initialDelay) {
        fadeCompleteTime = SDL_GetTicks64();
    } else {
        // Start car animation only once after fade
        if (!animatingCar && !carBitmap) {
            StartAnimateCar(state.GetCurrentCar(), nullptr, CarAnimationType::SINGLE_CAR_DRIVE);
            animatingCar = true;
        }
    }
}

void Garage::PushEvent(SDL_Event* event) {
    if (carSelectDialog && carSelectDialog->IsVisible()) {
        carSelectDialog->HandleEvent(event);
        return;  // Skip normal zone handling when dialog is visible
    }

    CarScene::PushEvent(event);
}

void Garage::OnCarAnimationComplete() {
    SDL_Log("Garage::OnCarAnimationComplete - Entry point");
    CarScene::OnCarAnimationComplete();
    if (pendingCar != nullptr) {
        state.SetCurrentCar(pendingCar);
        // Get or create bitmap for pending car
        CarBitmap* newBitmap = GetOrCreateCarBitmap(pendingCar);
        
        // Update scene's carBitmap pointer
        carBitmap = newBitmap;
        
        carx = -GAME_WIDTH;
        cary = SrLibQueryGetCarRoofYOffset(state.GetCurrentGameScene(), pendingCar->GetModel().index);
        carBitmap->Move({carx, cary});
        
        UpdateSign();
        
        
        animatingCar = true;
        currentAnimationType = CarAnimationType::SINGLE_CAR_DRIVE;
        //carAnimationTime = 0;
        
        pendingCar = nullptr;
    }
}

void Garage::InitCarBitmaps() {
    // Create bitmaps for all owned cars
    for (auto& car : state.GetOwnedCars()) {
        if (carBitmaps.find(car) == carBitmaps.end()) {
            carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
        }
    }
}

CarBitmap* Garage::GetOrCreateCarBitmap(Car* car) {
    auto it = carBitmaps.find(car);
    if (it == carBitmaps.end()) {
        carBitmaps[car] = std::make_unique<CarBitmap>(car, nullptr, car->GetColor());
    }
    return carBitmaps[car].get();
}

Garage::~Garage() {
    carBitmaps.clear();
}