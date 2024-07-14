#pragma once

#include <SDL.h>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>
#include <memory>
#include <map>

#include "bitmap.h"
#include "parsers/libp.h"


struct Asset {
    LibAssetType type;
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

    Bitmap* GetBitmapAsset(const Asset& asset);


private:
    vector<pair<Asset, unique_ptr<Bitmap>>> imageAssets;
    map<AssetLocation, pair<int, vector<BitmapEntry_t>>> imageEntries;
};