#include "assets.h"

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"

AssetManager::AssetManager() {
}

AssetManager::~AssetManager() {
}

void AssetManager::LoadAssets() {
    BitmapParser p;
    pair<int, vector<BitmapEntry_t>> entries = p.GetEntries(LIBP_FILE);
    imageEntries[AssetLocationAmigaLibP] = entries;

    Palette pal = p.PreparePalette(PAL1);

    for (int i = 0; i < entries.first; i++) {
        BitmapEntry_t b = entries.second[i];
        vector<uint8_t> data = p.ReadImage(LIBP_FILE, b, i, entries.first);
        vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);

        Asset a;
        a.id = i;
        a.location = AssetLocationAmigaLibP;
        a.type = p.GetAssetType(AssetLocationAmigaLibP, i);

        if (a.type == LibAssetSpritesheet) {
            pair<int, int> info = p.GetSpritesheetInfo(AssetLocationAmigaLibP, i);
            unique_ptr<Spritesheet> spriteSheet = make_unique<Spritesheet>(info.first, info.second);
            spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height);
            spriteSheet->SetDimensions(b.width, b.height);
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
    switch (asset.type) {
        case LibAssetRegular:
            for (auto& i : imageAssets) {
                if (i.first == asset) {
                    return i.second.get();
                }
            }
            break;

        default:
            break;
    }
    return nullptr;
}
