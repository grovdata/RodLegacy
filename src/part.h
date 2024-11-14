#pragma once
#include "parsers/libp.h"


class Part {
    private:
        int wear;
        int cost;
        CarVendor vendor;
        AmigaLibAsset assetId;
    public:
        int GetWear() const { return wear; }
        int GetCost() const { return cost; }
        AmigaLibAsset GetAssetId() const { return assetId; }

        Part(AmigaLibAsset assetId);
};