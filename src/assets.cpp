#include "assets.h"


#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/adf.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "embedded_loader.h"

// Generated embedded resources
extern "C" {
    #include "embedded_resources.h"
}

AssetManager::AssetManager() : carDecals(nullptr), audioManager(nullptr) {

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
    if (newspaperFont) {
        SDL_FreeSurface(newspaperFont->Surface);
        free(newspaperFont);
    }
    if (audioManager) {
        delete audioManager;
    }
}

void AssetManager::InitFonts() {
    SDL_Log("Loading fonts from embedded resources");
    
    // Load main font from embedded data
    SDL_Surface* mainFontSurface = LoadSurfaceFromMemory(
        embedded_font_png, 
        embedded_font_png_len
    );
    if (!mainFontSurface) {
        throw AssetLoadError("Failed to load embedded main font");
    }
    mainFont = SFont_InitFont(mainFontSurface);
    if (!mainFont) {
        SDL_FreeSurface(mainFontSurface);
        throw AssetLoadError("Failed to initialize main font");
    }
    
    // Load newspaper font from embedded data
    SDL_Surface* newspaperFontSurface = LoadSurfaceFromMemory(
        embedded_bee_png,
        embedded_bee_png_len
    );
    if (!newspaperFontSurface) {
        throw AssetLoadError("Failed to load embedded newspaper font");
    }
    newspaperFont = SFont_InitFont(newspaperFontSurface);
    if (!newspaperFont) {
        SDL_FreeSurface(newspaperFontSurface);
        throw AssetLoadError("Failed to initialize newspaper font");
    }
    
    // Load debug font from embedded data
    SDL_Surface* debugFontSurface = LoadSurfaceFromMemory(
        embedded_lemon_png,
        embedded_lemon_png_len
    );
    if (!debugFontSurface) {
        throw AssetLoadError("Failed to load embedded debug font");
    }
    debugFont = SFont_InitFont(debugFontSurface);
    if (!debugFont) {
        SDL_FreeSurface(debugFontSurface);
        throw AssetLoadError("Failed to initialize debug font");
    }
    
    SDL_Log("All fonts loaded successfully from embedded resources");
}

void AssetManager::InitMousePointers() {
    uint16_t* srExeData_uint16 = reinterpret_cast<uint16_t*>(srExeData_vec.data());

    for (int i = 0; i < SrLibAmigaSpriteAssetCount; i++) {
        SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[i];
        std::vector<SrLibColor> buffer(info.width * info.height);
        SrLibAmigaSpriteAsset pointer = (SrLibAmigaSpriteAsset)i;
        SrLibAmigaSpriteReadMousePointer(srExeData_uint16, pointer, buffer.data());
        std::unique_ptr<Bitmap> mousePointer = std::make_unique<Bitmap>();
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
        throw AssetLoadError("SR1AMIGA_DISK1.adf is corrupted (expected checksum: " + std::to_string(SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]) + ", actual checksum: " + std::to_string(disk1Status) + ")");
    }

    SrLibStatus disk2Status = SrLibVerifyChecksum(disk2Path.string().c_str(), SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk2]);
    if (disk2Status != SrLibStatusSuccess) {
        throw AssetLoadError("SR1AMIGA_DISK2.adf is corrupted (expected checksum: " + std::to_string(SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk2]) + ", actual checksum: " + std::to_string(disk2Status) + ")");
    }

    AdfFileData *srExeData = extract_adf_file_to_memory(disk1Path.string().c_str(), "Street_Rod");
    if (!srExeData) {
        throw AssetLoadError("Failed to extract Street_Rod executable from ADF");
    }
    srExeData_vec.assign(
        static_cast<uint8_t*>(srExeData->data),
        static_cast<uint8_t*>(srExeData->data) + srExeData->size
    );
    free_adf_file_data(srExeData);

    AdfFileData *libStartData = extract_adf_file_to_memory(disk1Path.string().c_str(), "libstart.p");
    if (!libStartData) {
        throw AssetLoadError("Failed to extract libstart.p from ADF");
    }
    libStartData_vec.assign(
        static_cast<uint8_t*>(libStartData->data),
        static_cast<uint8_t*>(libStartData->data) + libStartData->size
    );
    free_adf_file_data(libStartData);

    AdfFileData *libPData = extract_adf_file_to_memory(disk2Path.string().c_str(), "lib.p");
    if (!libPData) {
        throw AssetLoadError("Failed to extract lib.p from ADF");
    }
    libPData_vec.assign(
        static_cast<uint8_t*>(libPData->data),
        static_cast<uint8_t*>(libPData->data) + libPData->size
    );
    free_adf_file_data(libPData);
    
    AdfFileData *srData = extract_adf_file_to_memory(disk2Path.string().c_str(), "SR_Data");
    if (!srData) {
        throw AssetLoadError("Failed to extract SR_Data from ADF");
    }
    srData_vec.assign(
        static_cast<uint8_t*>(srData->data),
        static_cast<uint8_t*>(srData->data) + srData->size
    );
    free_adf_file_data(srData);

    SDL_Log("Found all required Amiga game files.");
}

void AssetManager::InitStockPalettes() {
    stockPalettes[StockPaletteAmigaDiner] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDiner), true);
    stockPalettes[StockPaletteAmigaGasStation] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaGasStation), true);
    stockPalettes[StockPaletteAmigaDriving] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDriving), true);
}

void AssetManager::InitAudio() {
    SDL_Log("Initializing audio system");
    
    audioManager = new AudioManager();
    
    fs::path disk1Path = ResolvePath(SrLibArchiveNames[SrLibArchiveSR1AmigaDisk1]);
    
    if (!audioManager->Initialize(disk1Path.string().c_str())) {
        SDL_Log("Warning: Audio initialization failed, continuing without audio");
        delete audioManager;
        audioManager = nullptr;
    } else {
        SDL_Log("Audio system initialized successfully");
    }
}

void AssetManager::Init(const fs::path& validatedPath) {
    try {
        gameBasePath = validatedPath;        
        dataPath = gameBasePath;  // All game files are directly in the game directory
        SDL_Log("Using game directory: %s", gameBasePath.string().c_str());
        SDL_Log("Data path: %s", dataPath.string().c_str());
        SDL_Log("Data path empty? %s", dataPath.empty() ? "YES" : "NO");
        InitFonts();
        InitGameFiles();
        InitStockPalettes();
        InitAudio();
    } catch (const AssetLoadError& e) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize asset manager: %s", e.what());
        throw;
    }

}

SFont_Font* AssetManager::GetMainFont() {
    return mainFont;
}

SFont_Font* AssetManager::GetDebugFont() {
    return debugFont;
}

SFont_Font* AssetManager::GetNewspaperFont() {
    return newspaperFont;
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
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);
    Palette pal = bp.PreparePalette(palData, true);
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette, index);

    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);

    bool isMask = a.type == LibAssetMask;
    bool isTransparent = bp.IsTransparent(SrLibFileAmigaLibP, index);

    if (a.type == LibAssetSpritesheet) {
        std::pair<int, int> info = bp.GetSpritesheetInfo(SrLibFileAmigaLibP, index);
        std::unique_ptr<Spritesheet> spriteSheet = std::make_unique<Spritesheet>(info.first, info.second);
        spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, isTransparent);
        spriteSheet->SetDimensions(b.width, b.height);
        imageAssets.push_back(std::make_pair(a, std::move(spriteSheet)));
    } else {
        std::unique_ptr<Bitmap> bitmap = std::make_unique<Bitmap>();
        bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, isTransparent);
        bitmap->SetDimensions(b.width, b.height);
        imageAssets.push_back(std::make_pair(a, std::move(bitmap)));
    }
}


void AssetManager::UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette) {
    std::vector<uint16_t> carPalette = GetStandardCarColor(palette);
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

Bitmap* AssetManager::GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette) {
    // Always create a new asset with the custom palette
    UpdateAssetWithPalette(libfile, asset.id, carPalette);
    return imageAssets.back().second.get();  // Return the newly created asset
}

void AssetManager::UpdateAssetWithPalette(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette) {
    BitmapEntry_t b = imageEntries.second[index];
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);
    
    // Convert car palette to RGB format
    Palette processedCarPalette = bp.PrepareCarPalette(carPalette);
    Palette basePal = bp.PreparePalette(palData, true);
    
    // Use provided car palette for color ranges
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette, index);
    
    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);
    
    std::unique_ptr<Bitmap> bitmap = std::make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height);
    bitmap->SetDimensions(b.width, b.height);
    imageAssets.push_back(std::make_pair(a, std::move(bitmap)));
}

fs::path AssetManager::FindGameDirectory() {
    // First try loading from saved config
    fs::path savedPath = LoadSavedGamePath();
    if (!savedPath.empty() && ValidateGamePath(savedPath)) {
        SDL_Log("Using saved game directory: %s", savedPath.string().c_str());
        return savedPath;
    }

    std::vector<fs::path> searchPaths = {
        fs::current_path(),
        fs::current_path() / "..",
        fs::current_path() / ".." / "..",
    };

    // Add environment variable path if set
    const char* envPath = getenv("STREETROD_DATA");
    if (envPath != nullptr) {
        searchPaths.push_back(fs::path(envPath));
    }

    for (const auto& path : searchPaths) {
        if (ValidateGamePath(path)) {
            // Save the valid path for future use
            SaveGamePath(path);
            return path;
        }
    }

    // If we get here, no valid path was found - now we can log the details
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "No valid game directory found in search paths:");
    for (const auto& path : searchPaths) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Checked path: %s", path.string().c_str());
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

bool AssetManager::ValidateGamePath(const fs::path& path) {
    std::vector<fs::path> gameFiles = {
        "SR1AMIGA_DISK1.adf",
        "SR1AMIGA_DISK2.adf",
        "font.png",
        "bee.png",
        "lemon.png",
        "icon.png"
    };

    bool hasAllFiles = true;

    for (const auto& file : gameFiles) {
        if (!fs::exists(path / file)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Missing file: %s", (path / file).string().c_str());
            hasAllFiles = false;
        }
    }

    return hasAllFiles;
}

void AssetManager::SetCustomPath(const std::string& customPath) {
    fs::path path = fs::path(customPath);
    
    if (!ValidateGamePath(path)) {
        throw AssetLoadError("Selected folder does not contain required game files");
    }
    
    gameBasePath = path;
    
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

fs::path AssetManager::LoadSavedGamePath() {
    char *prefPath = SDL_GetPrefPath("se.grovdata", "opensr");
    if (!prefPath) {
        return fs::path();
    }
    
    fs::path configPath = fs::path(prefPath) / "config.txt";
    SDL_free(prefPath);
    
    if (!fs::exists(configPath)) {
        return fs::path();
    }

    SDL_Log("Loaded config file: %s", configPath.c_str());
    
    try {
        std::ifstream config(configPath);
        std::string savedPath;
        std::getline(config, savedPath);
        
        if (!savedPath.empty() && fs::exists(savedPath)) {           
            return fs::path(savedPath);
        }
    } catch (...) {
        // Ignore read errors and continue with search
    }
    
    return fs::path();
}

void AssetManager::SaveGamePath(const fs::path& path) {
    char *prefPath = SDL_GetPrefPath("se.grovdata", "opensr");
    if (!prefPath) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get preference path to save game location");
        return;
    }
    
    try {
        fs::path configDir(prefPath);
        fs::path configPath = configDir / "config.txt";
        
        std::ofstream config(configPath, std::ios::out | std::ios::trunc);
        config << path.string();
        
        SDL_Log("Saved config file to: %s", configPath.string().c_str());
    } catch (const std::exception& e) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to save game path: %s", e.what());
    }
    
    SDL_free(prefPath);
}

