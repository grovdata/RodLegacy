#pragma once
#include <string>
#include <vector>

// Car sets
// --------
// A car set is a pack of cars the game can draw from. The Amiga stock set is
// mandatory and always sits first, which keeps its car-spec indices at 0..25 -
// the range original Street Rod .SAV files store - so those saves keep loading
// whatever else is switched on. Optional sets (data disks) append after it in
// registry order.
//
// Adding a set should be a matter of adding a CarSetDef here plus a loader
// that fills in CarSprites; nothing downstream should need to know which set
// a car came from beyond CarModel::carSet.

enum class CarSetArt {
    AmigaLibP,  // recoloured through the car palette
    DosLib,     // fixed 16-colour DOS art that carries its own paint
};

struct CarSetDef {
    std::string id;        // stable key, safe to write into a save
    std::string name;      // shown to the player
    bool        mandatory; // the stock set cannot be switched off
    CarSetArt   art;

    // Where the set's data lives, relative to the game data directory. Empty
    // for the Amiga set, which comes out of the already-loaded lib.p/SR_data.
    std::string zipName;    // e.g. "srsepc.zip"
    std::string libEntry;   // path inside the zip, e.g. "SRSE/LIB2"
    std::string dataEntry;  // path inside the zip, e.g. "SRSE/HOT_DATA"
    std::string looseDir;   // directory name when unzipped beside the game data
};

// Registry order is the merged car-spec table order, so it must stay stable:
// index 0 is the Amiga set, and new sets go on the end.
const std::vector<CarSetDef>& GetCarSetDefs();

// Index of the mandatory Amiga stock set. Always 0.
static constexpr int kAmigaCarSet = 0;

// Y of a car's wheels relative to the top of its body sprite, by car set and
// the set's own body sprite number.
//
// No car data file carries this - not the Amiga SR_data sprite-offset record,
// which has an X for each tire but no Y, and not its DOS counterpart. See
// RodLegacyResearch/docs/car_wheel_offsets.md. The values are measured by hand
// with the CarDebug scene; kDefaultCarWheelY is what an unmeasured car gets.
static constexpr int kDefaultCarWheelY = 30;
int CarSetWheelY(int carSet, int bodySprite);

// Where each car's sprites live in its set's lib, resolved when the model is
// built so nothing downstream has to know the set's conventions. -1 means the
// car has no such variant.
struct CarSprites {
    int body = -1;
    int choppedRoof = -1;
    int airScoop = -1;
    int rearBumperStripped = -1;
    int frontBumperStripped = -1;
};
