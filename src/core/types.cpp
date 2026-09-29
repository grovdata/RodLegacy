#include "core/types.h"

bool operator<(const CarModel &a, const CarModel &b) {
    return a.name < b.name;
}

CarSprites AmigaCarSprites(int bodyAsset) {
    AmigaLibAsset asset = (AmigaLibAsset)bodyAsset;
    CarSprites sprites;
    sprites.body = bodyAsset;
    // lib.p keeps a car's chopped roof directly after its body.
    if (CarQuery::SrLibQueryCanChopRoof(asset)) {
        sprites.choppedRoof = bodyAsset + 1;
    }
    if (CarQuery::SrLibQueryCanStripBumpers(asset)) {
        int rear = CarQuery::SrLibQueryGetChoppedBumberRearIndex(asset);
        int front = CarQuery::SrLibQueryGetChoppedBumberFrontIndex(asset);
        if (rear > 0) sprites.rearBumperStripped = rear;
        if (front > 0) sprites.frontBumperStripped = front;
    }
    // The hood scoop comes from the sprite-ref table, so the caller fills it in.
    return sprites;
}
