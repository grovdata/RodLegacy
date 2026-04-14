#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <tuple>
#include <memory>
#include <map>
#include <filesystem>
#include <stdexcept>

#include "bitmap.h"
#include "spritesheet.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "SFont.h"
#include "audio/audio_manager.h"

struct Asset {
    LibAssetType type;
    SrLibFile location;
    int id;

    bool operator==(const Asset& other) const {
        return (type == other.type && location == other.location && id == other.id);
    }
};

// Cache key for palette variants: combines asset ID with palette hash
struct AssetPaletteKey {
    int assetId;
    size_t paletteHash;  // Hash of the palette vector for fast comparison
    
    bool operator<(const AssetPaletteKey& other) const {
        if (assetId != other.assetId) return assetId < other.assetId;
        return paletteHash < other.paletteHash;
    }
    
    bool operator==(const AssetPaletteKey& other) const {
        return assetId == other.assetId && paletteHash == other.paletteHash;
    }
};

namespace fs = std::filesystem;

class AssetLoadError : public std::runtime_error {
public:
    AssetLoadError(const std::string& msg) : std::runtime_error(msg) {}
};

typedef std::vector<uint8_t> byte_vector;

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Static methods for game file validation
    static bool ValidateGamePath(const fs::path& path);
    static fs::path FindGameDirectory();
    static void SaveGamePath(const fs::path& path);

    // Instance methods
    void SetCustomPath(const std::string& customPath);
    void ReadAssets();

    void UpdateAsset(byte_vector& libfile, int index);
    void UpdateAsset(byte_vector& libfile, int index, Palette& carPalette);
    void UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette);
    Bitmap* GetBitmapAsset(const Asset& asset);
    Bitmap* GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette);
    const std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>>& GetMousePointers() const {
        return mousePointers;
    }
    Palette GetStockPalette(StockPalette palette);

    std::pair<int, std::vector<BitmapEntry_t>> GetImageEntries() { return imageEntries; }

    Spritesheet* GetCarDecals();

    Bitmap* GetDosDecal(int index);
    bool HasDosDecals() const;

    void Init(const fs::path& validatedPath, bool muteAudio = false);
    SFont_Font* GetMainFont();
    SFont_Font* GetDebugFont();
    SFont_Font* GetNewspaperFont();

    byte_vector& GetLibPData() { return libPData_vec; }
    byte_vector& GetLibStartData() { return libStartData_vec; }
    byte_vector& GetSrData() { return srData_vec; }
    byte_vector& GetSrExeData() { return srExeData_vec; }
    const byte_vector& GetDosSrExeUnpacked() const { return dosSrExeUnpacked_vec; }

    AudioManager* GetAudioManager() { return audioManager; }
    const fs::path& GetBasePath() const { return gameBasePath; }

    void LoadSettings();
    void SaveSettings();

private:
    fs::path gameBasePath;
    fs::path dataPath;

    BitmapParser bp;

    std::pair<int, std::vector<BitmapEntry_t>> imageEntries;
    std::vector<std::pair<Asset, std::unique_ptr<Bitmap>>> imageAssets;  // Base assets (no palette variants)
    std::map<AssetPaletteKey, std::unique_ptr<Bitmap>> paletteVariantCache;  // Cached palette variants
    std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>> mousePointers;

    Spritesheet* carDecals;

    byte_vector lib2Data_vec;
    std::pair<int, std::vector<DOSBitmapEntry_t>> dosImageEntries;
    std::vector<std::unique_ptr<Bitmap>> dosDecalBitmaps;

    SFont_Font* mainFont;
    SFont_Font* debugFont;
    SFont_Font* newspaperFont;

    byte_vector disk1Adf_vec;  // full disk1 ADF image bytes (from file or zip)
    byte_vector libStartData_vec;
    byte_vector libPData_vec;
    byte_vector srData_vec;
    byte_vector srExeData_vec;
    byte_vector dosSrExeUnpacked_vec;

    std::map<StockPalette, Palette> stockPalettes;

    AudioManager* audioManager;

    void InitFonts();
    void InitMousePointers();
    void InitGameFiles();
    void InitStockPalettes();
    void InitAudio(bool muteAudio = false);
    void InitDosAssets();
    fs::path ResolvePath(fs::path relativePath);
    void UpdateAssetWithPalette(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette);
    void CreateAndCachePaletteVariant(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey);


    static fs::path LoadSavedGamePath();
};
