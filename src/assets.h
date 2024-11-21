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
#include "parsers/carcolor.h"

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

    void ReadAssets(string libfile);

    void UpdateAsset(string libfile, int i);
    void UpdateAsset(string libfile, int i, Palette& carPalette);
    void UpdateAssetWithCarPalette(string libfile, int index, StandardCarColor palette);
    Bitmap* GetBitmapAsset(const Asset& asset);
    
    pair<int, vector<BitmapEntry_t>> GetImageEntries() { return imageEntries; }

   private:
    
    BitmapParser bp;

    pair<int, vector<BitmapEntry_t>> imageEntries;
    vector<pair<Asset, unique_ptr<Bitmap>>> imageAssets;
    
    
    //map<AssetLocation, pair<int, vector<BitmapEntry_t>>> imageEntries;

};