#include "assets.h"


#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

AssetManager::AssetManager() : carDecals(nullptr) {
    try {
        gameBasePath = FindGameDirectory();
        dataPath = gameBasePath / "data";
        fontPath = gameBasePath / "data" / "font";
        if (!fs::exists(dataPath)) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Data path does not exist: %s", dataPath.string().c_str());
            throw AssetLoadError("Data path does not exist: " + dataPath.string());
        }
        if (!fs::exists(fontPath)) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Font path does not exist: %s", fontPath.string().c_str());
            throw AssetLoadError("Font path does not exist: " + fontPath.string());
        }
        SDL_Log("Game directory found at: %s", gameBasePath.string().c_str());
    } catch (const AssetLoadError& e) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize asset manager: %s", e.what());
        throw;
    }
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

void AssetManager::ReadAssets(std::string libfile) {
    try {
        fs::path libPath = ResolvePath(libfile);
        imageEntries = bp.ReadEntries(libPath.string());

        for (int i = 0; i < imageEntries.first; i++) {
            UpdateAsset(libPath.string(), i);
        }

        if (carDecals == nullptr) {
            Spritesheet *decals = (Spritesheet*)GetBitmapAsset({LibAssetSpritesheet, AssetLocationAmigaLibP, DECALS});
            if (!decals) {
                throw AssetLoadError("Failed to load car decals");
            }
            SDL_Rect cropDecal = {7, 10, 215, 50};
            decals->Crop(cropDecal);
            carDecals = std::move(decals);
        }
    } catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to read assets: %s", e.what());
        throw;
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

fs::path AssetManager::FindGameDirectory() {
    // Get the base path where the executable is
    char* basePath = SDL_GetBasePath();
    if (!basePath) {
        throw AssetLoadError("Could not determine base path: " + std::string(SDL_GetError()));
    }
    fs::path base(basePath);
    SDL_free(basePath);

    // Check common locations for data directory
    std::vector<fs::path> searchPaths = {
        base / "data",           // Next to executable
        base / "../data",        // One level up
        base / "../../data",     // Two levels up (for build directories)
    };

    // Get the preferred path from environment variable if set
    const char* envPath = SDL_getenv("STREETROD_DATA");
    if (envPath) {
        searchPaths.insert(searchPaths.begin(), envPath);
    }

    // Search for data directory
    for (const auto& path : searchPaths) {
        if (fs::exists(path / "font/font.png")) {
            return path.parent_path(); // Return the game root directory
        }
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

fs::path AssetManager::ResolvePath(const std::string& relativePath) {
    fs::path fullPath = dataPath / relativePath;
    if (!fs::exists(fullPath)) {
        throw AssetLoadError("Required game file not found: " + fullPath.string());
    }
    return fullPath;
}

