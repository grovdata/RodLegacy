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

    const string GetDataFile();
    const string GetLibPFile();
    const string GetLibStartFile();

    void LoadAssets();

    void UpdateAsset(pair<int, vector<BitmapEntry_t>>& entries, int i);
    void UpdateAsset(pair<int, vector<BitmapEntry_t>>& entries, int i, Palette& carPalette);
    Bitmap* GetBitmapAsset(const Asset& asset);
    CarMetadata_t GetCarMetadata(int carIndex);

   private:
    DataParser dp;
    BitmapParser bp;

    vector<pair<Asset, unique_ptr<Bitmap>>> imageAssets;
    vector<CarMetadata_t> carMetadata;
    //map<AssetLocation, pair<int, vector<BitmapEntry_t>>> imageEntries;

};