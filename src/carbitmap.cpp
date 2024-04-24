#include "carbitmap.h"

CarBitmap::CarBitmap(Car *car) {
    this->car = car;
    CarModel model = car->GetModel();

    bodySurface = IMG_Load(model.filename.c_str());

    Uint32 topLeftPixelColor = *(Uint32*)bodySurface->pixels;
    if (SDL_SetColorKey(bodySurface, SDL_TRUE, topLeftPixelColor) != 0) {
        //...
    }

    bodyTexture = SDL_CreateTextureFromSurface(renderer, bodySurface);
    bodyRect.x = 0;
    bodyRect.y = 0;
    bodyRect.w = model.width;
    bodyRect.h = model.height;

    driverSurface = IMG_Load("../data/res/lib_p_346.gif");
    driverTexture = SDL_CreateTextureFromSurface(renderer, driverSurface);
    driverRect.x = bodyRect.x + model.driverX;
    driverRect.y = bodyRect.y + model.driverY;
    driverRect.w = driverSurface->w;
    driverRect.h = driverSurface->h;

    gasCap.LoadBitmap("../data/res/lib_p_308.gif");

    gasCap.SetVisible(true);

    hoodScoop.LoadBitmap("../data/res/lib_p_310.gif");
    hoodScoop.SetPosition(model.width + model.hoodScoopX, model.height + model.hoodScoopY);
    hoodScoop.SetVisible(true);

    rearWheel = new Spritesheet("../data/res/lib_p_321.png", 1, 4);
    frontWheel = new Spritesheet("../data/res/lib_p_321.png", 1, 4);
    rearWheel->SelectSprite(1,0);

    t = SDL_GetTicks64();
}

CarBitmap::~CarBitmap() {
    SDL_FreeSurface(bodySurface);
    SDL_DestroyTexture(bodyTexture);
    SDL_FreeSurface(driverSurface);
    SDL_DestroyTexture(driverTexture);
    delete rearWheel;
    delete frontWheel;
}

SDL_Texture* CarBitmap::GetTexture() {
    return bodyTexture;
}

void CarBitmap::Draw() {
    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        rearWheel->CycleNextSprite();
        frontWheel->CycleNextSprite();
    }


    SDL_RenderCopy(renderer, driverTexture, NULL, &driverRect);
    rearWheel->DrawSprite(&rearWheelRect);
    rearWheel->DrawSprite(&frontWheelRect);
    SDL_RenderCopy(renderer, bodyTexture, NULL, &bodyRect);

    gasCap.Draw();
    hoodScoop.Draw();
}

void CarBitmap::Move(SDL_Point p) {
    bodyRect.x = p.x;
    bodyRect.y = p.y;
    UpdateTextures();
}

void CarBitmap::MoveLeft() {
    bodyRect.x -= 3;
    UpdateTextures();
}

void CarBitmap::MoveRight() {
    bodyRect.x += 3;
    UpdateTextures();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    driverRect.x = bodyRect.x + model.driverX;
    driverRect.y = bodyRect.y + model.driverY;

    rearWheelRect = { bodyRect.x + model.rearWheelX, bodyRect.y + model.rearWheelY, 31, 31};
    frontWheelRect = { bodyRect.x + model.frontWheelX, bodyRect.y + model.frontWheelY, 31, 31};

    gasCap.SetPosition(bodyRect.x + model.gasCapX, bodyRect.y + model.gasCapY);
}