#include "assets.h"


#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/adf.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"

AssetManager::AssetManager() : carDecals(nullptr) {

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
    fs::path mainFontPath = fontPath / "font.png";
    fs::path debugFontPath = fontPath / "lemon.png";
    if (!fs::exists(mainFontPath) || !fs::exists(debugFontPath)) {
        throw AssetLoadError("Font files not found");
    }

    mainFont = SFont_InitFont(IMG_Load(mainFontPath.string().c_str()));
    debugFont = SFont_InitFont(IMG_Load(debugFontPath.string().c_str()));
}

void AssetManager::InitGameFiles() {
    fs::path disk1Path = ResolvePath(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk1]);
    printf("Disk1 path: %s\n", disk1Path.string().c_str());
    SrLibStatus srlibver = SrLibVerifyChecksum(disk1Path.string().c_str(), SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]);
    if (srlibver != SrLibStatusSuccess) {
        throw AssetLoadError("SR1AMIGA_DISK1.adf is corrupted (expected checksum: " + to_string(SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]) + ", actual checksum: " + to_string(srlibver) + ")");
    }

    AdfFileData *libStartData = extract_adf_file_to_memory(disk1Path.string().c_str(), "libstart.p");
    
    // Copy binary data to vector
    libStartData_vec.assign(
        static_cast<uint8_t*>(libStartData->data),
        static_cast<uint8_t*>(libStartData->data) + libStartData->size
    );
    delete libStartData;
    
    fs::path disk2Path = ResolvePath("SR1AMIGA_DISK2.adf");
    AdfFileData *libPData = extract_adf_file_to_memory(disk2Path.string().c_str(), "lib.p");
    libPData_vec.assign(
        static_cast<uint8_t*>(libPData->data),
        static_cast<uint8_t*>(libPData->data) + libPData->size
    );
    delete libPData;
    
    AdfFileData *srData = extract_adf_file_to_memory(disk2Path.string().c_str(), "SR_Data");
    srData_vec.assign(
        static_cast<uint8_t*>(srData->data),
        static_cast<uint8_t*>(srData->data) + srData->size
    );
    delete srData;
}

void AssetManager::InitStockPalettes() {
    stockPalettes[StockPaletteAmigaDiner] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDiner), true);
    stockPalettes[StockPaletteAmigaGasStation] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaGasStation), true);

    int i = 0;
    for (auto& pair : stockPalettes[StockPaletteAmigaDiner]) {
        SDL_Log("Diner palette %d: %d, %d, %d", i++, get<0>(pair), get<1>(pair), get<2>(pair));
    }
}

void AssetManager::Init() {
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

    InitFonts();
    InitGameFiles();
    InitStockPalettes();
}

SFont_Font* AssetManager::GetMainFont() {
    return mainFont;
}

SFont_Font* AssetManager::GetDebugFont() {
    return debugFont;
}

Palette AssetManager::GetStockPalette(StockPalette palette) {
    return stockPalettes[palette];
}

void AssetManager::ReadAssets() {
    try {
        imageEntries = bp.ReadEntries(libPData_vec);

        for (int i = 0; i < imageEntries.first; i++) {
            UpdateAsset(libPData_vec, i);
        }

        if (carDecals == nullptr) {
            Spritesheet *decals = (Spritesheet*)GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DECALS});
            if (!decals) {
                throw AssetLoadError("Failed to load car decals");
            }            
            SDL_Rect cropDecal = {4, 9, 222, 51};
            decals->Crop(cropDecal);
            carDecals = std::move(decals);
        }
    } catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to read assets: %s", e.what());
        throw;
    }
}

void AssetManager::UpdateAsset(byte_vector& libfile, int index) {
    Palette empty;
    UpdateAsset(libfile, index, empty);
}


/// @brief Reads the actual bitmap data for a bitmap
/// @param libfile
/// @param index
/// @param carPalette
void AssetManager::UpdateAsset(byte_vector& libfile, int index, Palette& carPalette) {
    BitmapEntry_t b = imageEntries.second[index];
    vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);
    Palette pal = bp.PreparePalette(palData, true);
    vector<Color> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette);

    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);

    bool isMask = a.type == LibAssetMask;

    if (a.type == LibAssetSpritesheet) {
        pair<int, int> info = bp.GetSpritesheetInfo(SrLibFileAmigaLibP, index);
        unique_ptr<Spritesheet> spriteSheet = make_unique<Spritesheet>(info.first, info.second);
        spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask);
        spriteSheet->SetDimensions(b.width, b.height);
        imageAssets.push_back(make_pair(a, std::move(spriteSheet)));
    } else {
        unique_ptr<Bitmap> bitmap = make_unique<Bitmap>();
        bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
        bitmap->SetDimensions(b.width, b.height);
        imageAssets.push_back(make_pair(a, std::move(bitmap)));
    }
}


void AssetManager::UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette) {
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

    SDL_LogWarn(0, "Warning: Asset not found (wrong type of asset?): %d", asset.id);
    return nullptr;
}

Spritesheet* AssetManager::GetCarDecals() {
    return this->carDecals;
}

Bitmap* AssetManager::GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const vector<uint16_t>& carPalette) {
    // Always create a new asset with the custom palette
    UpdateAssetWithPalette(libfile, asset.id, carPalette);
    return imageAssets.back().second.get();  // Return the newly created asset
}

void AssetManager::UpdateAssetWithPalette(byte_vector& libfile, int index, const vector<uint16_t>& carPalette) {
    BitmapEntry_t b = imageEntries.second[index];
    vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);
    
    // Convert car palette to RGB format
    Palette processedCarPalette = bp.PrepareCarPalette(carPalette);
    Palette basePal = bp.PreparePalette(palData, true);
    
    // Use provided car palette for color ranges
    vector<Color> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette);
    
    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);
    
    unique_ptr<Bitmap> bitmap = make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
    bitmap->SetDimensions(b.width, b.height);
    imageAssets.push_back(make_pair(a, std::move(bitmap)));
}

fs::path AssetManager::FindGameDirectory() {
    // Get the base path where the executable is
    char* basePath = SDL_GetBasePath();
    SDL_Log("Game base path: %s", basePath);
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
        SDL_Log("Using STREETROD_DATA from environment: %s", envPath);
        searchPaths.insert(searchPaths.begin(), envPath);
    }

    // Search for data directory
    for (const auto& path : searchPaths) {
        // Check if ADF files are in the data directory
        if (fs::exists(path / "data" / "SR1AMIGA_DISK1.adf") && 
            fs::exists(path / "data" / "SR1AMIGA_DISK2.adf")) {
            SDL_Log("Found game files in data subdirectory: %s", path.string().c_str());
            return path;
        }
        // Also check if ADF files are in the root directory
        if (fs::exists(path / "SR1AMIGA_DISK1.adf") && 
            fs::exists(path / "SR1AMIGA_DISK2.adf")) {
            SDL_Log("Found game files in: %s", path.string().c_str());
            return path;
        }
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

fs::path AssetManager::ResolvePath(fs::path relativePath) {
    fs::path fullPath = dataPath / relativePath;
    if (!fs::exists(fullPath)) {
        throw AssetLoadError("Required game file not found: " + fullPath.string());
    }
    return fullPath;
}

