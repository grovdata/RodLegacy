#include "assets.h"

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

AssetManager::AssetManager() {
    this->carDecals = nullptr;
}

AssetManager::~AssetManager() {
    if (mainFont) {
        SDL_FreeSurface(mainFont->Surface);
        free(mainFont);
    }
    if (debugFont) {
        SDL_FreeSurface(debugFont->Surface);
        free(debugFont);
    }
}

void AssetManager::InitFonts() {
    mainFont = SFont_InitFont(IMG_Load("../data/font/font.png"));
    debugFont = SFont_InitFont(IMG_Load("../data/font/lemon.png"));
}

SFont_Font* AssetManager::GetMainFont() {
    return mainFont;
}

SFont_Font* AssetManager::GetDebugFont() {
    return debugFont;
}

void AssetManager::ReadAssets(string libfile) {
    imageEntries = bp.ReadEntries(libfile);

    for (int i = 0; i < imageEntries.first; i++) {
        UpdateAsset(libfile, i);
    }

    if (carDecals == nullptr) {
        Spritesheet *decals = (Spritesheet*)GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, DECALS});
        SDL_Rect cropDecal = {7, 10, 215, 50};
        decals->Crop(cropDecal); // Remove the decal border (window)
        carDecals = std::move(decals);
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

Spritesheet* AssetManager::GetCarDecals() {
    return this->carDecals;
}

Bitmap* AssetManager::GetBitmapAssetWithPalette(string libfile, Asset asset, const vector<uint16_t>& carPalette) {
    // Always create a new asset with the custom palette
    UpdateAssetWithPalette(libfile, asset.id, carPalette);
    return imageAssets.back().second.get();  // Return the newly created asset
}

void AssetManager::UpdateAssetWithPalette(string libfile, int index, const vector<uint16_t>& carPalette) {
    BitmapEntry_t b = imageEntries.second[index];
    vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    vector<uint16_t> palData = bp.GetPalette(AssetLocationAmigaLibP, index);
    
    // Convert car palette to RGB format
    Palette processedCarPalette = bp.PrepareCarPalette(carPalette);
    Palette basePal = bp.PreparePalette(palData, true);
    
    // Use provided car palette for color ranges
    vector<Color> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette);
    
    Asset a;
    a.id = index;
    a.location = AssetLocationAmigaLibP;
    a.type = bp.GetAssetType(AssetLocationAmigaLibP, index);
    
    unique_ptr<Bitmap> bitmap = make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
    bitmap->SetDimensions(b.width, b.height);
    imageAssets.push_back(make_pair(a, std::move(bitmap)));
}

