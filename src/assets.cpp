#include "assets.h"

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"

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

    offsets = dp.GetEntries(GetDataFile());

    pair<int, vector<BitmapEntry_t>> entries = bp.GetEntries(GetLibPFile());
    //cout << "entries" << entries.first << " " << GetLibPFile() << endl;

    Palette pal = bp.PreparePalette(PAL1);

    for (int i = 0; i < entries.first; i++) {
        BitmapEntry_t b = entries.second[i];
        vector<uint8_t> data = bp.ReadImage(GetLibPFile(), b, i, entries.first);
        vector<Color> chunkyData = bp.PlanarToChunky(data, b, pal);

        Asset a;
        a.id = i;
        a.location = AssetLocationAmigaLibP;
        a.type = bp.GetAssetType(AssetLocationAmigaLibP, i);

        if (a.type == LibAssetSpritesheet) {
            pair<int, int> info = bp.GetSpritesheetInfo(AssetLocationAmigaLibP, i);
            unique_ptr<Spritesheet> spriteSheet = make_unique<Spritesheet>(info.first, info.second);
            spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height);
            //spriteSheet->SetDimensions(b.width, b.height);
            imageAssets.push_back(make_pair(a, move(spriteSheet)));
        } else {
            unique_ptr<Bitmap> bitmap = make_unique<Bitmap>();
            bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
            bitmap->SetDimensions(b.width, b.height);
            imageAssets.push_back(make_pair(a, move(bitmap)));
        }
    }
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