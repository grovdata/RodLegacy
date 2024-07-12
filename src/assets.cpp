#include "assets.h"

#include "bitmap.h"
#include "parsers/libp.h"

AssetManager::AssetManager() {
}

AssetManager::~AssetManager() {
}

void AssetManager::LoadAssets() {
    BitmapParser p;
    pair<int, vector<BitmapEntry_t>> entries = p.GetEntries(LIBP_FILE);

    Palette pal = p.PreparePalette(PAL1);

    for (int i = 0; i < entries.first; i++) {
        BitmapEntry_t b = entries.second[i];
        vector<uint8_t> data = p.ReadImage(LIBP_FILE, b, i, entries.first);
        vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);
        Bitmap bitmap;
        bitmap.LoadBitmapFromChunky(chunkyData, b.width, b.height);

        Asset a;
        a.id = i;
        a.location = AssetLocationAmigaLibP;
        a.type = AssetTypeBitmap;

        imageAssets.push_back(make_pair(a, bitmap));
    }
}

Bitmap AssetManager::GetBitmapAsset(Asset asset) {
    switch (asset.type) {
        case AssetTypeBitmap:
            for (auto i: imageAssets) {
                if (i.first == asset) {
                    return i.second;
                }
            }

        default:
            break;
    }
}
