#include "car/car_set.h"

#include <map>

namespace {

// Measured with the CarDebug scene against the drawn car, keyed by the set's
// own body sprite number. Anything absent sits at kDefaultCarWheelY, which is
// a fallback and not a measurement - see the "still on the default" list in
// RodLegacyResearch/docs/car_wheel_offsets.md.

// Amiga lib.p car body assets.
const std::map<int, int> kAmigaWheelY = {
    {179, 37},  // 1940 Chevrolet 2-dr coupe
    {183, 39},  // 1940 Chevrolet Roadster
    {187, 40},  // 1938 Chevrolet Master Deluxe 2-dr
    {191, 24},  // 1956 Corvette
    {194, 23},  // 1961 Corvette
    {197, 29},  // 1963 Corvette
    {204, 38},  // 1932 Ford deuce coupe
    {214, 35},  // 1932 Ford Sedan
    {216, 39},  // 1940 Ford Deluxe 2-dr
    {224, 31},  // 1957 Ford Fairlane 500 2-dr
    {228, 32},  // 1954 Mercury Monterey
    {232, 33},  // 1956 Mercury Custom 2-dr
    {236, 37},  // 1949 Chevrolet 2dr Styleline
    {244, 34},  // 1949 Olds 88 2-dr
    {248, 31},  // 1952 Olds 88
    {252, 36},  // 1950 Pontiac Silver Streak
    {260, 32},  // 1961 Valiant V-200
    {264, 31},  // 1963 Valiant V-100 2-dr
    {268, 33},  // 1955 Chevrolet Bel-Air Sport coupe
};

// Street Rod SE LIB2 body sprites. These numbers overlap the Amiga asset ids,
// which is why the lookup is per set.
const std::map<int, int> kSrseWheelY = {
    { 39, 34},  // 1939 Pontiac 2-dr coupe
    { 40, 36},  // 1946 Chevrolet Fleetline Sedan
    { 53, 28},  // 1955 Oldsmobile 98
    { 54, 29},  // 1957 Chevrolet Bel-Air
    { 60, 26},  // 1958 Buick Special
    { 61, 27},  // 1959 Cadillac Eldorado Convertible
    {113, 24},  // 1961 Chevrolet Impala
    {118, 29},  // 1962 Oldsmobile Starfire
    {119, 27},  // 1963 Buick Riviera
    {126, 36},  // 1934 Ford Woodie
    {127, 37},  // 1936 Ford Coupe
    {131, 31},  // 1950 Ford Crestliner
    {135, 34},  // 1951 Mercury Monterey
    {145, 31},  // 1957 Lincoln Capri
    {149, 25},  // 1960 Ford Thunderbird
    {156, 28},  // 1963 Ford Galaxie 500XL
    {157, 28},  // 1963 Ford Falcon Sprint
    {164, 26},  // 1963 Ford T-bird Roadster
    {165, 33},  // 1948 Chrysler Town & Country Conv.
    {171, 27},  // 1956 Chrysler New Yorker
    {172, 27},  // 1957 Plymouth Fury
    {179, 24},  // 1960 Plymouth Convertible
};

} // namespace

int CarSetWheelY(int carSet, int bodySprite) {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    if (carSet < 0 || carSet >= (int)defs.size()) return kDefaultCarWheelY;

    const std::map<int, int>* table = nullptr;
    if (defs[carSet].id == "amiga") {
        table = &kAmigaWheelY;
    } else if (defs[carSet].id == "srse") {
        table = &kSrseWheelY;
    }
    if (!table) return kDefaultCarWheelY;

    auto it = table->find(bodySprite);
    return it == table->end() ? kDefaultCarWheelY : it->second;
}

const std::vector<CarSetDef>& GetCarSetDefs() {
    static const std::vector<CarSetDef> defs = {
        {
            "amiga", "Street Rod (stock)", true, CarSetArt::AmigaLibP,
            "", "", "", ""
        },
        {
            "srse", "Street Rod SE", false, CarSetArt::DosLib,
            "srsepc.zip", "SRSE/LIB2", "SRSE/HOT_DATA", "SRSE"
        },
    };
    return defs;
}
