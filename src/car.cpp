#include "car.h"

Car::Car(CarModel model) : isFrontBumperChopped(false), isBackBumperChopped(false), isRoofChopped(false), m_model(model), m_tires(STOCK) {
    bodySurface = IMG_Load(model.filename.c_str());
    bodyTexture = SDL_CreateTextureFromSurface(renderer, bodySurface);
    bodyRect.x = 0;
    bodyRect.y = 0;
    bodyRect.w = model.width;
    bodyRect.h = model.height;


    driverSurface = IMG_Load("../data/rip/lib_p_346.gif");
    driverTexture = SDL_CreateTextureFromSurface(renderer, driverSurface);
    // TODO: Get driver position/dimension
    driverRect.x = bodyRect.x + 102;
    driverRect.y = bodyRect.y + 17;
    driverRect.w = 24;
    driverRect.h = 17;

    rearWheel = new Spritesheet("../data/rip/lib_p_321.png", 1, 4);
    frontWheel = new Spritesheet("../data/rip/lib_p_321.png", 1, 4);
    rearWheel->SelectSprite(1,0);

    t = SDL_GetTicks64();
}

Car::~Car() {
    SDL_FreeSurface(bodySurface);
    SDL_DestroyTexture(bodyTexture);
    SDL_FreeSurface(driverSurface);
    SDL_DestroyTexture(driverTexture);
    delete rearWheel;
    delete frontWheel;
}

SDL_Texture* Car::GetTexture() {
    return bodyTexture;
}

void Car::Draw() {
    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        rearWheel->CycleNextSprite();
        frontWheel->CycleNextSprite();
    }

    SDL_RenderCopy(renderer, driverTexture, NULL, &driverRect);
    rearWheel->DrawSprite(&rearWheelRect);
    rearWheel->DrawSprite(&frontWheelRect);
    SDL_RenderCopy(renderer, bodyTexture, NULL, &bodyRect);
}

void Car::Move(SDL_Point p) {
    bodyRect.x = p.x;
    bodyRect.y = p.y;
    UpdateTextures();
}

void Car::MoveLeft() {
    bodyRect.x -= 3;
    UpdateTextures();
}

void Car::MoveRight() {
    bodyRect.x += 3;
    UpdateTextures();
}

void Car::UpdateTextures() {
    // TODO: Get driver position/dimension
    driverRect.x = bodyRect.x + 102;
    driverRect.y = bodyRect.y + 3;

    rearWheelRect = { bodyRect.x + 30, bodyRect.y + 38, 31, 31};
    frontWheelRect = { bodyRect.x + 180, bodyRect.y + 38, 31, 31};
}