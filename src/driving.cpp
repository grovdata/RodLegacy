#include "driving.h"

Driving::Driving() : Scene("../data/res/lib_p_11.gif") {
    // dashboard = new Bitmap();
    // dashboard->LoadBitmap("../data/res/lib_p_11.gif");
}

Driving::~Driving() {

}

void Driving::Draw() {
    Scene::Draw();
    // dashboard->Draw();
}