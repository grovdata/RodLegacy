/*
 * RodLegacy - The newspaper scene: front page, used-car and auto-parts classifieds, and buying.
 *
 * Created: 2025-01-23
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

#pragma once

#include "scenes/scene.h"
#include "gfx/bitmap.h"
#include "ui/label.h"
#include "parsers/libp.h"
#include "dialogs/dialog.h"
#include "car/car_system.h"
#include "car/part.h"
#include <vector>
#include <memory>

// Name and price come from the parsed ClassifiedEntry_t; only the part's identity is hardcoded here.
struct PartCatalogEntry {
    PartCategory category;
    CarVendor_t vendor;
    CarEngine_t engineType;
    TransmissionType transType;
    CarburetorType carbType;
    ManifoldType manifoldType;
    CarWheelType tireType;
    int sr2Class;        // SrExhaustManifold / SrMuffler / SrDifferential
};

// The catalogue's own name for a part, as the original's car info prints it. Empty when there is none.
std::string CatalogPartName(PartCategory category, CarVendor_t vendor, int typeValue);

class Newspaper : public Scene {
private:
    Bitmap exit;
    Bitmap headline;

    Bitmap previous;
    Bitmap next;

    Bitmap adsArea;
    Bitmap allAdsSurface;
    SDL_Rect adsViewport;

    Label date;
    // The ad list must stay index-aligned with state.classifiedCars.
    uint32_t lastClassifiedsGeneration = 0;
    std::string lastDateText;  // detects a changed game date (e.g. after loading a save) to refresh `date`
    int headlineIndex = 0;

    std::vector<Label> classifiedAds;
    std::vector<Label> usedCarAds;

    int currentPage = 0;
    int totalPages = 0;      // used-car pages
    int totalPartsPages = 0; // AUTO PARTS pages
    int adsPerPage = 6;

    std::vector<CarMetadata_t> carMetadata;
    std::vector<int> carAssetIds; // Parallel to carMetadata: the AmigaLibAsset index

    // Parallel to kPartCatalog.
    std::vector<PartMetadata_t> partMetadata;

    int allAdsSurfaceHeight = 0;
    int partsSectionHeight = 0; // measured height of the AUTO PARTS block, set by CreateAllAdsSurface

    int smoothScrollOffset = 0;

    struct AdPosition {
        int adIndex;        // Index into carMetadata (isUsedCar) or partMetadata/kPartCatalog (!isUsedCar)
        int startY;
        int endY;
        bool isUsedCar;
    };
    std::vector<AdPosition> adPositions;

    std::unique_ptr<Dialog> carPurchaseDialog;
    std::unique_ptr<Dialog> carViewDialog;
    std::unique_ptr<Dialog> partPurchaseDialog;
    int selectedCarIndex = -1;
    int lastClickX = 0;
    int lastClickY = 0;

    void InitPageOne();
    void SetHeadline(int asset);
    void InitPageAds();
    void LoadClassifiedsData();
    void CreateClassifiedAds();
    void CreateUsedCarAds();
    using Scene::Update; // un-hide Scene::Update(Uint64); this Update() is unrelated (page content refresh)
    void Update();
    void Update(Uint64 deltaTime) override;
    void UpdatePageContent();
    void RefreshClassifieds();
    void AddDivider(int y);
    void HandleAdClick(int adIndex);
    void CreateAllAdsSurface();
    void UpdateViewport();
    void SaveAdsSurfaceToFile();
    int FindAdAtPosition(int mouseY);
    void ShowCarPurchaseDialog(int carIndex);
    void ShowCarViewDialog(int carIndex);
    void ShowPartPurchaseDialog(int partIndex);

    // Carb and tire ads are not vendor-specific, so vendor is ignored for those.
    std::string FindCatalogName(PartCategory category, CarVendor_t vendor, int typeValue) const;
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event *event) override;
    std::vector<SDL_Rect> NavTargets() const override;

    // Newspaper is constructed once, so this reopens it on the front page.
    void ResetToFrontPage();

    // The newspaper's own wording for parts not bought there. "" if the type/vendor is not in the catalog.
    std::string FindEngineCatalogName(CarEngine_t type, CarVendor_t vendor) const;
    std::string FindTransmissionCatalogName(TransmissionType type, CarVendor_t vendor) const;
    std::string FindCarburetorCatalogName(CarburetorType type) const;
    std::string FindManifoldCatalogName(ManifoldType type, CarVendor_t vendor) const;
    std::string FindTireCatalogName(CarWheelType type) const;
};
