#pragma once

#include <SDL.h>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

#include "bitmap.h"

enum AssetType {
    AssetTypeBitmap = 0
};

enum AssetLocation {
    AssetLocationAmigaLibStart = 0,
    AssetLocationAmigaLibP = 1
};

struct Asset {
    AssetType type;
    AssetLocation location;
    int id;

    bool operator==(const Asset& other) const {
        return (type == other.type && location == other.location && id == other.id);
    }
};

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    void LoadAssets();

    Bitmap GetBitmapAsset(Asset asset);


private:
    vector<pair<Asset, Bitmap>> imageAssets;

    // Load a single surface
    SDL_Surface* loadSurface(const std::string& filePath);
};