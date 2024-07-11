#include "carbitmap.h"
#include "parsers/libp.h"
CarBitmap::CarBitmap(Car *car) {
    this->car = car;
    CarModel model = car->GetModel();

    // bodySurface = IMG_Load(model.filename.c_str());

    // Get the top-left pixel's color
    // Uint32 topLeftPixelColor = ((Uint32*)bodySurface->pixels)[0];

    // SDL_Log("debugpix %d", topLeftPixelColor);
    // if (SDL_SetColorKey(bodySurface, SDL_TRUE, topLeftPixelColor) != 0) {
    //     SDL_LogDebug(0, "SDL_SetColorKey error %s", SDL_GetError());
    // }


    int spriteIndex = 179;
    string examplefile = "../../streetrod-bins/amiga/SR_disk2/lib.p";
    BitmapParser p;
    pair<int, vector<BitmapEntry_t>> entries = p.GetEntries(examplefile.c_str());
    BitmapEntry_t b = entries.second[spriteIndex];
    vector<uint8_t> data = p.ReadImage(examplefile.c_str(), b, spriteIndex, entries.first);
    Palette pal = p.PreparePalette(PAL1);

    vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);
    // // p.PrintBitplanes(data);

    //bodySurface = createSurfaceFromColors(chunkyData, b.width, b.height);
    body.LoadBitmapFromChunky(chunkyData, b.width, b.height);
    body.SetPosition(0, 0);
    body.SetDimensions(model.width, model.height);
    body.SetVisible(true);
    SDL_Rect bodyRect = body.GetPosition();

    // bodyTexture = SDL_CreateTextureFromSurface(renderer, bodySurface);
    // bodyRect.x = 0;
    // bodyRect.y = 0;
    // bodyRect.w = model.width;
    // bodyRect.h = model.height;

    driverSurface = IMG_Load("../data/res/lib_p_346.gif");
    driverTexture = SDL_CreateTextureFromSurface(renderer, driverSurface);
    driverRect.x = bodyRect.x + model.driverX;
    driverRect.y = bodyRect.y + model.driverY;
    driverRect.w = driverSurface->w;
    driverRect.h = driverSurface->h;

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
    // delete body;
    // SDL_DestroyTexture(bodyTexture);
    SDL_FreeSurface(driverSurface);
    SDL_DestroyTexture(driverTexture);
    delete rearWheel;
    delete frontWheel;
}

// SDL_Texture* CarBitmap::GetTexture() {
//     return body.texture;
// }

void CarBitmap::Draw() {
    Uint64 elapsed = SDL_GetTicks64() - t;

    if (elapsed % 400 == 0) {
        rearWheel->CycleNextSprite();
        frontWheel->CycleNextSprite();
    }


    SDL_RenderCopy(renderer, driverTexture, NULL, &driverRect);
    rearWheel->DrawSprite(&rearWheelRect);
    rearWheel->DrawSprite(&frontWheelRect);
    // SDL_RenderCopy(renderer, bodyTexture, NULL, &bodyRect);
    body.Draw();

    gasCap.Draw();
    hoodScoop.Draw();
}

void CarBitmap::Move(SDL_Point p) {
    body.SetPosition(p.x, p.y);
    UpdateTextures();
}

void CarBitmap::UpdateTextures() {
    CarModel model = car->GetModel();
    SDL_Rect bodyRect = body.GetPosition();
    driverRect.x = bodyRect.x + model.driverX;
    driverRect.y = bodyRect.y + model.driverY;

    rearWheelRect = { bodyRect.x + model.rearWheelX, bodyRect.y + model.rearWheelY, 31, 31};
    frontWheelRect = { bodyRect.x + model.frontWheelX, bodyRect.y + model.frontWheelY, 31, 31};

    gasCap.SetPosition(bodyRect.x + model.gasCapX, bodyRect.y + model.gasCapY);
}
