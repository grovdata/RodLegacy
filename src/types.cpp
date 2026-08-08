#include "types.h"

bool operator<(const CarModel &a, const CarModel &b) {
    return a.name < b.name;
}

bool CarModelIsSrse(const CarModel &model) {
    return model.spriteSource == SrLibFileSrseLib2;
}

int CarBodySprite(const CarModel &model) {
    // SE models carry an index shifted clear of lib.p's car assets so it stays
    // a unique pool key, so the real sprite number comes off dosSprites.
    return CarModelIsSrse(model) ? model.dosSprites.body : model.index;
}

int CarChoppedRoofSprite(const CarModel &model) {
    if (CarModelIsSrse(model)) return model.dosSprites.choppedRoof;
    AmigaLibAsset body = (AmigaLibAsset)model.index;
    return CarQuery::SrLibQueryCanChopRoof(body) ? model.index + 1 : -1;
}

int CarRearBumperSprite(const CarModel &model) {
    if (CarModelIsSrse(model)) return model.dosSprites.rearBumperStripped;
    AmigaLibAsset body = (AmigaLibAsset)model.index;
    if (!CarQuery::SrLibQueryCanStripBumpers(body)) return -1;
    int index = CarQuery::SrLibQueryGetChoppedBumberRearIndex(body);
    return index > 0 ? index : -1;
}

int CarFrontBumperSprite(const CarModel &model) {
    if (CarModelIsSrse(model)) return model.dosSprites.frontBumperStripped;
    AmigaLibAsset body = (AmigaLibAsset)model.index;
    if (!CarQuery::SrLibQueryCanStripBumpers(body)) return -1;
    int index = CarQuery::SrLibQueryGetChoppedBumberFrontIndex(body);
    return index > 0 ? index : -1;
}
