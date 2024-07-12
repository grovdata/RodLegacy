#include "garage.h"
#include "state.h"
#include "assets.h"
#include "state.h"

Garage::Garage() : CarScene((Asset){AssetTypeBitmap, AssetLocationAmigaLibP, GARAGE_SCREEN}) {

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

    Bubble *bubbleDrink = new Bubble("  Don\'t drink and drive, bub!", BUBBLE_RIGHT, 79, 30, true);
    bubbleDrink->SetVisible(false);
    bubbles.push_back(bubbleDrink);
    Bubble *partsBubble = new Bubble("  You\'ve got no spare parts, dummy!", BUBBLE_RIGHT, 79, 30, true);
    partsBubble->SetVisible(false);
    bubbles.push_back(partsBubble);
    Bubble *tiresBubble = new Bubble("  You\'ve got no spare tires, dummy!", BUBBLE_RIGHT, 79, 30, true);
    tiresBubble->SetVisible(false);
    bubbles.push_back(tiresBubble);


    signSurface = SDL_CreateRGBSurface(0, GAME_WIDTH, GAME_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    StartAnimateCar(state.GetCurrentCar(), nullptr, false);
}

MousePointer Garage::GetPointerType() {
    return PointerWrench;
}

SDL_Point Garage::GetCarIdlePosition() {
    return {35, 82};
}

SDL_Point Garage::GetCarOffscreenPosition() {
    return {-320, 82};
}

void Garage::UpdateSign() {
    char text[50];
    CarModel m = state.GetCurrentCar()->GetModel();
    snprintf(text, 50, "%d %s.", m.year, m.name.c_str());
    SFont_Write(signSurface, font, 85, 45, text);
    signTexture = SDL_CreateTextureFromSurface(renderer, signSurface);
    SDL_SetTextureColorMod(signTexture, 0, 0, 0);
}

void Garage::Action(ZoneId zone) {
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    switch (zone)
    {
    case ZONE_GARAGE_GOTO_DINER:
        sceneTransitioningTo = SCENE_CRUISING;
        FadeOut();
        break;
    case ZONE_GARAGE_GAS_CAN:
        sceneTransitioningTo = SCENE_GAS_STATION;
        FadeOut();
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
    default:
        break;
    }
}

void Garage::Draw() {
    Scene::Draw();

    UpdateSign();
    SDL_RenderCopy(renderer, signTexture, NULL, &m_backgroundRect);

    for (auto b : bubbles) {
        b->Draw();
    }
}