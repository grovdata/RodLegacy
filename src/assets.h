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
    AssetLocation location;
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

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    fs::path FindGameDirectory();
    void ReadAssets(string libfile);

    void UpdateAsset(string libfile, int i);
    void UpdateAsset(string libfile, int i, Palette& carPalette);
    void UpdateAssetWithCarPalette(string libfile, int index, StandardCarColor palette);
    Bitmap* GetBitmapAsset(const Asset& asset);
    Bitmap* GetBitmapAssetWithPalette(string libfile, Asset asset, const vector<uint16_t>& carPalette);

    pair<int, vector<BitmapEntry_t>> GetImageEntries() { return imageEntries; }

    Spritesheet* GetCarDecals();

    void InitFonts();
    SFont_Font* GetMainFont();
    SFont_Font* GetDebugFont();

private:
    fs::path gameBasePath;
    fs::path dataPath;
    fs::path fontPath;

    BitmapParser bp;


    pair<int, vector<BitmapEntry_t>> imageEntries;
    vector<pair<Asset, unique_ptr<Bitmap>>> imageAssets;

    Spritesheet* carDecals;

    SFont_Font* mainFont;
    SFont_Font* debugFont;

    std::vector<uint8_t> libStartData_vec;
    std::vector<uint8_t> libPData_vec;
    std::vector<uint8_t> srData_vec;

    void OpenGameFiles();
    fs::path ResolvePath(fs::path relativePath);
    void UpdateAssetWithPalette(string libfile, int index, const vector<uint16_t>& carPalette);
};