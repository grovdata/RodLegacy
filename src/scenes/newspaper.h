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

// One of the 40 fixed AUTO PARTS classified slots (see LoadClassifiedsData /
// kPartCatalog in newspaper.cpp). Name and price always come from the parsed
// ClassifiedEntry_t, not from here -- only category/vendor/specific type are
// hardcoded, since PartMetadata_t carries ad text only, not part identity.
struct PartCatalogEntry {
    PartCategory category;
    CarVendor_t vendor;
    CarEngine_t engineType;
    TransmissionType transType;
    CarburetorType carbType;
    ManifoldType manifoldType;
    CarWheelType tireType;
};

class Newspaper : public Scene {
private:
    Bitmap exit;
    Bitmap headline;

    Bitmap previous;
    Bitmap next;

    Bitmap adsArea;
    Bitmap allAdsSurface; // Large surface containing all ads
    SDL_Rect adsViewport; // Current viewport into the large surface

    // Page 0
    Label date;
    std::string lastDateText;  // detects a changed game date (e.g. after loading a save) to refresh `date`
    // Which of the five front-page slabs is on the paper right now.
    int headlineIndex = 0;

    // Headers are now part of the large surface, no individual labels needed

    // Dynamic classifieds
    std::vector<Label> classifiedAds;
    std::vector<Label> usedCarAds;

    // Current page data
    int currentPage = 0;
    int totalPages = 0;      // used-car pages
    int totalPartsPages = 0; // AUTO PARTS pages
    int adsPerPage = 6; // Number of ads that fit on one page (multi-line ads)

    // Car metadata for classifieds
    std::vector<CarMetadata_t> carMetadata;
    std::vector<int> carAssetIds; // Parallel to carMetadata: the AmigaLibAsset index

    // AUTO PARTS classifieds: ad text/price parsed from the Street_Rod exe,
    // parallel to the fixed kPartCatalog category table (newspaper.cpp).
    std::vector<PartMetadata_t> partMetadata;

    // Surface dimensions
    int allAdsSurfaceHeight = 0;
    int partsSectionHeight = 0; // measured height of the AUTO PARTS block, set by CreateAllAdsSurface

    // Smooth scrolling
    int smoothScrollOffset = 0;

    // Ad position mapping
    struct AdPosition {
        int adIndex;        // Index into carMetadata (isUsedCar) or partMetadata/kPartCatalog (!isUsedCar)
        int startY;         // Start Y position in the large surface
        int endY;           // End Y position in the large surface
        bool isUsedCar;     // true for used car ads, false for AUTO PARTS ads
    };
    std::vector<AdPosition> adPositions;

    // Dialog system
    std::unique_ptr<Dialog> carPurchaseDialog;
    std::unique_ptr<Dialog> carViewDialog;
    std::unique_ptr<Dialog> partPurchaseDialog;
    int selectedCarIndex = -1;
    int lastClickX = 0;
    int lastClickY = 0;

    void InitPageOne();
    void SetHeadline(AmigaLibAsset asset);
    void InitPageAds();
    void LoadClassifiedsData();
    void CreateClassifiedAds();
    void CreateUsedCarAds();
    using Scene::Update; // un-hide Scene::Update(Uint64); this Update() is unrelated (page content refresh)
    void Update();
    void Update(Uint64 deltaTime) override; // the real per-frame entry point (fade + date refresh)
    void UpdatePageContent();
    void AddDivider(int y);
    void HandleAdClick(int adIndex);
    void CreateAllAdsSurface();
    void UpdateViewport();
    void SaveAdsSurfaceToFile();
    int FindAdAtPosition(int mouseY);
    void ShowCarPurchaseDialog(int carIndex);
    void ShowCarViewDialog(int carIndex);
    void ShowPartPurchaseDialog(int partIndex);

    // Shared lookup behind the FindXCatalogName wrappers below - scans the
    // fixed 40-slot kPartCatalog for a category+vendor+type match and
    // returns that slot's classified-ad name. Carb/tire ads aren't
    // vendor-specific (their kPartCatalog entries always carry
    // CarVendorUnknown), so vendor is ignored for those two categories.
    std::string FindCatalogName(PartCategory category, CarVendor_t vendor, int typeValue) const;
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode) override;
    void DrawScene() override;
    bool PushEvent(SDL_Event *event) override;

    // Newspaper is a persistent scene object (constructed once at startup),
    // so currentPage would otherwise still be wherever the player last left
    // it. Called from State::MoveToScene every time the player walks back
    // in, so it always opens on the front page.
    void ResetToFrontPage();

    // Authentic classified-ad name for a part, keyed by its logical type
    // (+vendor where the ad text is vendor-specific) - lets other screens
    // (e.g. the garage's spare-parts pickers) match the newspaper's own
    // wording for parts that weren't bought there (stock/savegame parts,
    // whose Part::name is empty) instead of falling back to a generic
    // description. Returns "" if this exact type/vendor never appears in
    // the fixed 40-slot classified catalog.
    std::string FindEngineCatalogName(CarEngine_t type, CarVendor_t vendor) const;
    std::string FindTransmissionCatalogName(TransmissionType type, CarVendor_t vendor) const;
    std::string FindCarburetorCatalogName(CarburetorType type) const;
    std::string FindManifoldCatalogName(ManifoldType type, CarVendor_t vendor) const;
    std::string FindTireCatalogName(CarWheelType type) const;
};
