#pragma once

#include <SDL.h>
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

struct Asset {
    LibAssetType type;
    SrLibFile location;
    int id;

    bool operator==(const Asset& other) const {
        return (type == other.type && location == other.location && id == other.id);
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
    Bitmap* GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const vector<uint16_t>& carPalette);
    const std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>>& GetMousePointers() const {
        return mousePointers;
    }
    Palette GetStockPalette(StockPalette palette);

    pair<int, vector<BitmapEntry_t>> GetImageEntries() { return imageEntries; }

    Spritesheet* GetCarDecals();

    void Init(const fs::path& validatedPath);
    SFont_Font* GetMainFont();
    SFont_Font* GetDebugFont();
    SFont_Font* GetNewspaperFont();

    byte_vector& GetLibPData() { return libPData_vec; }
    byte_vector& GetLibStartData() { return libStartData_vec; }
    byte_vector& GetSrData() { return srData_vec; }

private:
    fs::path gameBasePath;
    fs::path dataPath;
    fs::path fontPath;

    BitmapParser bp;

    pair<int, vector<BitmapEntry_t>> imageEntries;
    vector<pair<Asset, unique_ptr<Bitmap>>> imageAssets;
    map<SrLibAmigaSpriteAsset, unique_ptr<Bitmap>> mousePointers;

    Spritesheet* carDecals;

    SFont_Font* mainFont;
    SFont_Font* debugFont;
    SFont_Font* newspaperFont;

    byte_vector libStartData_vec;
    byte_vector libPData_vec;
    byte_vector srData_vec;
    byte_vector srExeData_vec;

    map<StockPalette, Palette> stockPalettes;

    void InitFonts();
    void InitMousePointers();
    void InitGameFiles();
    void InitStockPalettes();
    fs::path ResolvePath(fs::path relativePath);
    void UpdateAssetWithPalette(byte_vector& libfile, int index, const vector<uint16_t>& carPalette);

    static bool ValidateGamePathQuiet(const fs::path& path);
    
    static fs::path LoadSavedGamePath();
};