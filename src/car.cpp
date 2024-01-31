#include "car.h"

Car::Car(CarModel model) : m_frontBumperChopped(false), m_backBumperChopped(false), m_roofChopped(false), m_model(model), m_tires(STOCK) {
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
}

Car::~Car() {
    SDL_FreeSurface(bodySurface);
    SDL_DestroyTexture(bodyTexture);
    SDL_FreeSurface(driverSurface);
    SDL_DestroyTexture(driverTexture);
}

SDL_Texture* Car::GetTexture() {
    return bodyTexture;
}

void Car::Draw() {
    SDL_RenderCopy(renderer, driverTexture, NULL, &driverRect);
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
}