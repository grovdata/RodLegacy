#include "assets.h"

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

AssetManager::AssetManager() {
}

AssetManager::~AssetManager() {
}


void AssetManager::ReadAssets(string libfile) {    
    imageEntries = bp.ReadEntries(libfile);

    for (int i = 0; i < imageEntries.first; i++) {
        UpdateAsset(libfile, i);
    }
}

/// @brief Reads the actual bitmap data for a bitmap
/// @param libfile 
/// @param index 
/// @param carPalette 
void AssetManager::UpdateAsset(string libfile, int index, Palette& carPalette) {
    BitmapEntry_t b = imageEntries.second[index];
    vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
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

void AssetManager::UpdateAsset(string libfile, int index) {
    Palette empty;
    UpdateAsset(libfile, index, empty);
}

void AssetManager::UpdateAssetWithCarPalette(string libfile, int index, StandardCarColor palette) {
    vector<uint16_t> carPalette = GetStandardCarColor(palette);   
    Palette newPalette = bp.PrepareCarPalette(carPalette);
    UpdateAsset(libfile, index, newPalette); // Update the asset with the new color
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

