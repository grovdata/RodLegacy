#include "driving.h"

Driving::Driving() : Scene("") {
    Bitmap *dashboard;
    Bitmap *pillar;
    Bitmap *roof;
    Bitmap *hood;
    Bitmap *mirrorStem;
    Bitmap *mirror;

    dashboard = new Bitmap();
    dashboard->LoadBitmap("../data/res/lib_p_11.gif");
    dashboard->Align(BitmapAlignBottom, true);
    dashboard->SetVisible(true);

    pillar = new Bitmap();
    pillar->LoadBitmap("../data/res/lib_p_18.gif");
    pillar->AlignAbove(dashboard);
    pillar->SetVisible(true);

    roof = new Bitmap();
    roof->LoadBitmap("../data/res/lib_p_20.gif");
    roof->AlignAbove(pillar);
    roof->SetVisible(true);

    hood = new Bitmap();
    hood->LoadBitmap("../data/res/lib_p_19.gif");
    hood->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    hood->SetVisible(true);

    mirrorStem = new Bitmap();
    mirrorStem->LoadBitmap("../data/res/lib_p_41.gif");
    mirrorStem->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    mirrorStem->SetVisible(true);

    mirror = new Bitmap();
    mirror->LoadBitmap("../data/res/lib_p_42.gif");
    mirror->SetPosition(72, GAME_HEIGHT - 107 - GAME_STATUSBAR_HEIGHT);
    mirror->SetVisible(true);



    // Order is important for rendering
    interior.push_back(roof);
    interior.push_back(pillar);
    interior.push_back(hood);
    interior.push_back(dashboard);
}

Driving::~Driving() {
    for (auto b : interior) {
        delete b;
    }
}

void Driving::Draw() {
    Scene::Draw();
    for (auto b : interior) {
        b->Draw();
    }
}
