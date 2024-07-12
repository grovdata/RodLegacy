#include "carbitmap.h"
#include "parsers/libp.h"
#include "assets.h"
#include "state.h"

CarBitmap::CarBitmap(Car *car) {
    this->car = car;
    CarModel model = car->GetModel();

    Asset carAsset = {AssetTypeBitmap, AssetLocationAmigaLibP, FORD_FAIRLANE_19XX_BODY};
    body = state.assets.GetBitmapAsset(carAsset);

    body->SetPosition(0, 0);
    body->SetDimensions(model.width, model.height);
    body->SetVisible(true);
    SDL_Rect bodyRect = body->GetPosition();

    driverSurface = IMG_Load("../data/res/lib_p_346.gif");
    driverTexture = SDL_CreateTextureFromSurface(renderer, driverSurface);
    driverRect.x  = model.driverX;
    driverRect.y  = model.driverY;
    driverRect.w  = driverSurface->w;
    driverRect.h  = driverSurface->h;

    gasCap.LoadBitmap("../data/res/lib_p_308.gif");

    gasCap.SetVisible(false);

    hoodScoop.LoadBitmap("../data/res/lib_p_310.gif");
    hoodScoop.SetPosition(model.width + model.hoodScoopX, model.height + model.hoodScoopY);
    hoodScoop.SetVisible(true);

    rearWheel = new Spritesheet("../data/res/lib_p_321.png", 1, 4);
    frontWheel = new Spritesheet("../data/res/lib_p_321.png", 1, 4);
    rearWheel->SelectSprite(1,0);

    t = SDL_GetTicks64();
}

CarBitmap::~CarBitmap() {
    SDL_FreeSurface(driverSurface);
    SDL_DestroyTexture(driverTexture);
    delete rearWheel;
    delete frontWheel;
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

    body->Draw();
    gasCap.Draw();
    hoodScoop.Draw();
}

void CarBitmap::Move(SDL_Point p) {
    body->SetPosition(p.x, p.y);
    UpdateTextures();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    SDL_Rect bodyRect = body->GetPosition();
    driverRect.x = bodyRect.x + model.driverX;
    driverRect.y = bodyRect.y + model.driverY;

    rearWheelRect = { bodyRect.x + model.rearWheelX, bodyRect.y + model.rearWheelY, 31, 31};
    frontWheelRect = { bodyRect.x + model.frontWheelX, bodyRect.y + model.frontWheelY, 31, 31};

    gasCap.SetPosition(bodyRect.x + model.gasCapX, bodyRect.y + model.gasCapY);
}
