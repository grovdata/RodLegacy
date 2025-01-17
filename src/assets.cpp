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

void AssetManager::InitMousePointers() {
    uint16_t* srExeData_uint16 = reinterpret_cast<uint16_t*>(srExeData_vec.data());

    for (int i = 0; i < SrLibAmigaSpriteAssetCount; i++) {
        SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[i];
        vector<SrLibColor> buffer(info.width * info.height);
        SrLibAmigaSpriteAsset pointer = (SrLibAmigaSpriteAsset)i;
        SrLibAmigaSpriteReadMousePointer(srExeData_uint16, pointer, buffer.data());
        unique_ptr<Bitmap> mousePointer = make_unique<Bitmap>();
        mousePointer->LoadBitmapFromChunky(buffer, info.width, info.height);
        mousePointer->SetDimensions(info.width, info.height);
        mousePointers[pointer] = std::move(mousePointer);
    }
}

void AssetManager::InitGameFiles() {
    fs::path disk1Path = ResolvePath(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk1]);
    fs::path disk2Path = ResolvePath(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk2]);

    SrLibStatus disk1Status = SrLibVerifyChecksum(disk1Path.string().c_str(), SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]);
    if (disk1Status != SrLibStatusSuccess) {
        throw AssetLoadError("SR1AMIGA_DISK1.adf is corrupted (expected checksum: " + to_string(SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]) + ", actual checksum: " + to_string(disk1Status) + ")");
    }

    SrLibStatus disk2Status = SrLibVerifyChecksum(disk2Path.string().c_str(), SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk2]);
    if (disk2Status != SrLibStatusSuccess) {
        throw AssetLoadError("SR1AMIGA_DISK2.adf is corrupted (expected checksum: " + to_string(SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk2]) + ", actual checksum: " + to_string(disk2Status) + ")");
    }

    AdfFileData *srExeData = extract_adf_file_to_memory(disk1Path.string().c_str(), "Street_Rod");
    srExeData_vec.assign(
        static_cast<uint8_t*>(srExeData->data),
        static_cast<uint8_t*>(srExeData->data) + srExeData->size
    );
    delete srExeData;

    AdfFileData *libStartData = extract_adf_file_to_memory(disk1Path.string().c_str(), "libstart.p");
    
    libStartData_vec.assign(
        static_cast<uint8_t*>(libStartData->data),
        static_cast<uint8_t*>(libStartData->data) + libStartData->size
    );
    delete libStartData;

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

    SDL_Log("Found all required Amiga game files.");
}

void AssetManager::InitStockPalettes() {
    stockPalettes[StockPaletteAmigaDiner] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDiner), true);
    stockPalettes[StockPaletteAmigaGasStation] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaGasStation), true);
}

void AssetManager::Init(const fs::path& validatedPath) {
    try {
        gameBasePath = validatedPath;        
        dataPath = gameBasePath / "data";
        fontPath = gameBasePath / "data" / "font";
        SDL_Log("Using game directory: %s", gameBasePath.string().c_str());
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

        InitMousePointers();

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
    vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette);

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

    SDL_LogError(0, "Warning: Asset not found (wrong type of asset?): %d", asset.id);
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
    vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette);
    
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
    fs::path basePath;
    
    // Check for custom path from environment variable first
    const char* envPath = SDL_getenv("STREETROD_DATA");
    if (envPath) {
        SDL_Log("Using STREETROD_DATA from environment: %s", envPath);
        basePath = fs::path(envPath);
        // Check if this path contains the game files
        if (ValidateGamePath(basePath)) {
            return basePath;
        }
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "STREETROD_DATA path does not contain required files");
    }

    // Get the executable base path as fallback
    char* sdlBasePath = SDL_GetBasePath();
    if (!sdlBasePath) {
        throw AssetLoadError("Could not determine base path: " + std::string(SDL_GetError()));
    }
    basePath = fs::path(sdlBasePath);
    SDL_free(sdlBasePath);

    // Check common locations relative to executable
    std::vector<fs::path> searchPaths = {
        basePath,                // Executable directory
        basePath / "data",       // Next to executable
        basePath / "../data",    // One level up
        basePath / "../../data", // Two levels up (for build directories)
    };

    // Search for data directory
    for (const auto& path : searchPaths) {
        if (ValidateGamePath(path)) {
            return path;
        }
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

bool AssetManager::ValidateGamePath(const fs::path& path) {
    // Check for ADF files directly in the path
    bool hasAdfFiles = fs::exists(path / "SR1AMIGA_DISK1.adf") && 
                      fs::exists(path / "SR1AMIGA_DISK2.adf");
    
    // Check for required directory structure
    bool hasDataDir = fs::exists(path / "data");
    bool hasFontDir = fs::exists(path / "data" / "font");
    bool hasResDir = fs::exists(path / "data" / "res");
    
    // Check for required files
    bool hasFontFile = fs::exists(path / "data" / "font" / "font.png");
    bool hasIconFile = fs::exists(path / "data" / "res" / "icon.png");

    // Check if all requirements are met
    if (hasAdfFiles && hasDataDir && hasFontDir && hasResDir && hasFontFile && hasIconFile) {
        SDL_Log("Found valid game directory at: %s", path.string().c_str());
        return true;
    }

    // If not found directly, check if ADF files are in data subdirectory
    hasAdfFiles = fs::exists(path / "data" / "SR1AMIGA_DISK1.adf") && 
                 fs::exists(path / "data" / "SR1AMIGA_DISK2.adf");
    
    if (hasAdfFiles && hasDataDir && hasFontDir && hasResDir && hasFontFile && hasIconFile) {
        SDL_Log("Found valid game directory with ADFs in data subdirectory: %s", path.string().c_str());
        return true;
    }

    // Log what's missing for debugging purposes
    if (!hasAdfFiles) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing ADF files");
    if (!hasDataDir) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing data directory");
    if (!hasFontDir) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing font directory");
    if (!hasResDir) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing res directory");
    if (!hasFontFile) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing font.png");
    if (!hasIconFile) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing icon.png");
    
    return false;
}

void AssetManager::SetCustomPath(const std::string& customPath) {
    fs::path path = fs::path(customPath);
    
    if (!ValidateGamePath(path)) {
        throw AssetLoadError("Selected folder does not contain required game files");
    }
    
    gameBasePath = path;
    dataPath = gameBasePath / "data";
    fontPath = gameBasePath / "data" / "font";
    
    // Create directories if needed
    if (!fs::exists(dataPath)) {
        fs::create_directory(dataPath);
    }
    if (!fs::exists(fontPath)) {
        fs::create_directories(fontPath);
    }
    
    SDL_Log("Using game directory: %s", gameBasePath.string().c_str());
    
    InitFonts();
    InitGameFiles();
    InitStockPalettes();
}

fs::path AssetManager::ResolvePath(fs::path relativePath) {
    fs::path fullPath = dataPath / relativePath;
    if (!fs::exists(fullPath)) {
        throw AssetLoadError("Required game file not found: " + fullPath.string());
    }
    return fullPath;
}

