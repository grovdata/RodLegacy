#include "assets.h"

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

AssetManager::AssetManager() {
}

AssetManager::~AssetManager() {
}

const string AssetManager::GetDataFile() {
    return "../../streetrod-bins/amiga/SR_disk2/SR_data";
}
const string AssetManager::GetLibPFile() {
    return "../../streetrod-bins/amiga/SR_disk2/lib.p";
}
const string AssetManager::GetLibStartFile() {
    return "../../streetrod-bins/amiga/SR_disk1/libstart.p";
}

void AssetManager::LoadAssets() {
    offsets = dp.ReadEntries(GetDataFile());
    pair<int, vector<BitmapEntry_t>> entries = bp.ReadEntries(GetLibPFile());

    for (int i = 0; i < entries.first; i++) {
        UpdateAsset(entries, i);
    }
}

void AssetManager::UpdateAsset(pair<int, vector<BitmapEntry_t>>& entries, int index, Palette& carPalette) {
    BitmapEntry_t b = entries.second[index];
    vector<uint8_t> data = bp.ReadImage(GetLibPFile(), b, index, entries.first);


    // FIXME hardcoded
    if (index == 179) {
        vector<uint16_t> turq = PAL_CAR_COLOR_LIGHT_GREEN;
        carPalette = bp.PrepareCarPalette(turq);
    }

    vector<uint16_t> palData = bp.GetPalette(AssetLocationAmigaLibP, index);
    Palette pal = bp.PreparePalette(palData, true);


    vector<Color> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette);

    Asset a;
    a.id = index;
    a.location = AssetLocationAmigaLibP;
    a.type = bp.GetAssetType(AssetLocationAmigaLibP, index);

    if (a.type == LibAssetSpritesheet) {
        pair<int, int> info = bp.GetSpritesheetInfo(AssetLocationAmigaLibP, index);
        unique_ptr<Spritesheet> spriteSheet = make_unique<Spritesheet>(info.first, info.second);
        spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height);
        spriteSheet->SetDimensions(b.width, b.height);
        imageAssets.push_back(make_pair(a, std::move(spriteSheet)));
    } else {

        unique_ptr<Bitmap> bitmap = make_unique<Bitmap>();
        bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
        bitmap->SetDimensions(b.width, b.height);
        imageAssets.push_back(make_pair(a, std::move(bitmap)));
    }
}

void AssetManager::UpdateAsset(pair<int, vector<BitmapEntry_t>>& entries, int index) {
    Palette empty;
    UpdateAsset(entries, index, empty);
}

Bitmap* AssetManager::GetBitmapAsset(const Asset& asset) {
    for (auto& i : imageAssets) {
        if (i.first == asset) {
            return i.second.get();
        }
    }

    cout << "Warning: Asset not found " << asset.id << endl;
    return nullptr;
}

BitmapOffset_t AssetManager::GetCarOffset(int carIndex) {
    int index = dp.GetCarIndex((AmigaLibAsset)carIndex);
    return offsets[index];
}