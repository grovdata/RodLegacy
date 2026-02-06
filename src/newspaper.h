#pragma once

#include "scene.h"
#include "bitmap.h"
#include "ui/label.h"
#include "parsers/libp.h"
#include "dialogs/dialog.h"
#include "carbitmap.h"
#include <vector>
#include <memory>

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
    Label headingLeft;
    Label headingRight;
    
    // Headers are now part of the large surface, no individual labels needed
    
    // Dynamic classifieds
    std::vector<Label> classifiedAds;
    std::vector<Label> usedCarAds;
    
    // Current page data
    int currentPage = 0;
    int totalPages = 0;
    int adsPerPage = 6; // Number of ads that fit on one page (multi-line ads)
    
    // Car metadata for classifieds
    std::vector<CarMetadata_t> carMetadata;
    
    // Surface dimensions
    int allAdsSurfaceHeight = 0;
    
    // Smooth scrolling
    int smoothScrollOffset = 0;
    
    // Ad position mapping
    struct AdPosition {
        int adIndex;        // Index in carMetadata
        int startY;         // Start Y position in the large surface
        int endY;           // End Y position in the large surface
        bool isUsedCar;     // true for used car ads, false for classified ads
    };
    std::vector<AdPosition> adPositions;

    // Dialog system
    std::unique_ptr<Dialog> carPurchaseDialog;
    std::unique_ptr<Dialog> carViewDialog;
    std::unique_ptr<Bitmap> carPreviewBitmap;
    int selectedCarIndex = -1;

    void InitPageOne();
    void InitPageAds();
    void LoadClassifiedsData();
    void CreateClassifiedAds();
    void CreateUsedCarAds();
    void Update();
    void UpdatePageContent();
    std::string WordWrap(const std::string& text, int maxLineLength);
    std::string FormatClassifiedAd(const ClassifiedEntry_t& entry, int index);
    std::string FormatUsedCarAd(const CarMetadata_t& metadata, int index);
    void AddDivider(int y);
    void HandleAdClick(int adIndex);
    void CreateAllAdsSurface();
    void UpdateViewport();
    void SaveAdsSurfaceToFile();
    int FindAdAtPosition(int mouseY);
    void ShowCarPurchaseDialog(int carIndex);
    void ShowCarViewDialog(int carIndex);
    void CreateCarPreviewBitmap(int carIndex);
    
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode) override;
    void Draw() override;
    void Action(ZoneId zone) override;
    bool PushEvent(SDL_Event *event) override;
};
