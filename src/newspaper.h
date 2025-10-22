#pragma once

#include "scene.h"
#include "bitmap.h"
#include "ui/label.h"
#include "parsers/libp.h"
#include <vector>

class Newspaper : public Scene {
private:
    Bitmap exit;
    Bitmap headline;
    
    Bitmap previous;
    Bitmap next;

    Bitmap adsArea;

    // Page 0
    Label date;
    Label headingLeft;
    Label headingRight;
    
    // Page 1-x
    Label autoPartsHeader;
    Label usedCarsHeader;
    
    // Dynamic classifieds
    std::vector<Label> classifiedAds;
    std::vector<Label> usedCarAds;
    
    // Current page data
    int currentPage = 0;
    int totalPages = 0;
    int adsPerPage = 6; // Number of ads that fit on one page (multi-line ads)
    
    // Car metadata for classifieds
    std::vector<CarMetadata_t> carMetadata;

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
    
public:
    Newspaper();
    ~Newspaper();

    void Init(bool debugMode) override;
    void Draw() override;
    void Action(ZoneId zone) override;
    void PushEvent(SDL_Event *event) override;
};
