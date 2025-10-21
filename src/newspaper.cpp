#include "newspaper.h"
#include "state.h"

const std::string autoParts = "------------ PARTS ------------";
const std::string usedCars = "---------- USED CARS ----------";

Newspaper::Newspaper() : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU}) {
    currentPage = 0;
}

Newspaper::~Newspaper() {

}

void Newspaper::Init(bool debugMode) {
    Scene::Init(debugMode);
    InitPageOne();
    LoadClassifiedsData();
    InitPageAds();

    Update();
    // Common

    SDL_Rect autoPartsRect = {0, 0, GAME_WIDTH, 32};
    Zone autoPartsZone(ZONE_MAGAZINE_AUTO_PARTS, strdup("Auto Parts"), &autoPartsRect);

    SDL_Rect usedCarsRect = {0, 33, GAME_WIDTH, 33};
    Zone usedCarsZone(ZONE_MAGAZINE_USED_CARS, strdup("Used Cars"), &usedCarsRect);

    SDL_Rect backToGarageRect = {0, 66, GAME_WIDTH, 125};
    Zone backToGarageZone(ZONE_MAGAZINE_BACK_TO_GARAGE, strdup("Go to the garage"), &backToGarageRect);

    SDL_Rect previousRect = {0, 0, 64, 190};
    Zone previousZone(ZONE_MAGAZINE_PREVIOUS, strdup("Previous page"), &previousRect);

    SDL_Rect nextRect = {GAME_WIDTH - 64, 0, 64, 190};
    Zone nextZone(ZONE_MAGAZINE_NEXT, strdup("Next page"), &nextRect);

    SDL_Rect backToGarage2Rect = {0, 199, GAME_WIDTH, 125};
    Zone backToGarage2Zone(ZONE_MAGAZINE_BACK_TO_GARAGE2, strdup("Go to the garage"), &backToGarage2Rect);

    zones.push_back(autoPartsZone);
    zones.push_back(usedCarsZone);
    zones.push_back(backToGarageZone);
    zones.push_back(previousZone);
    zones.push_back(nextZone);
    zones.push_back(backToGarage2Zone);
}

void Newspaper::InitPageOne() {
    previous = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    previous.SetPosition(0, 0);
    SDL_Rect previousRect = {0, 0, 64, 190};
    previous.Crop(previousRect);
    previous.SetDimensions(64, 190);
    previous.UpdateFromSurface();
    previous.SetVisible(false);

    next = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    next.SetPosition(GAME_WIDTH - 64, 0);
    SDL_Rect nextRect = {64, 0, 64, 190};
    next.Crop(nextRect);
    next.SetDimensions(64, 190);
    next.UpdateFromSurface();
    next.SetVisible(false);

    // Set headline based on date?
    AmigaLibAsset headlineAsset = NEWSPAPER_NO_SOLDIERS_IN_VIETNAM;
    headline = state.assets.GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, headlineAsset});
    headline.SetPosition(0, 136);
    headline.SetVisible(true); 

    const SDL_Rect masks[5][2] = {
        {{0, 0, 144, 16}, {146, 0, 175, 30}},
        {{0, 0, 144, 30}, {146, 0, 175, 30}},
        {{0, 0, 144, 30}, {146, 0, 175, 30}},
        {{0, 0, 145, 24}, {146, 0, 175, 24}},
        {{0, 0, 145, 32}, {146, 0, 175, 32}}
    };

    // Temp
    SDL_Rect headlineMaskRect[2];
    headlineMaskRect[0] = masks[headlineAsset - NEWSPAPER_DODGERS_MALIBU][0];
    headlineMaskRect[1] = masks[headlineAsset - NEWSPAPER_DODGERS_MALIBU][1];
    // SDL_Rect headlineMaskRect = {0, 0, 175, 30};
    SDL_FillRect(headline.GetSurface(), &headlineMaskRect[0], SDL_MapRGBA(headline.GetSurface()->format, 240, 240, 240, 255));
    SDL_FillRect(headline.GetSurface(), &headlineMaskRect[1], SDL_MapRGBA(headline.GetSurface()->format, 240, 240, 240, 255));
    headline.UpdateFromSurface();
    
    uint32_t dateColor = SDL_MapRGB(surface->format, 32, 32, 32);
    date.Init(state.assets.GetMainFont(), 0, 120, dateColor);
    date.SetText("  June 10, 1963  ", true, GAME_WIDTH);

    headingLeft.Init(state.assets.GetNewspaperFont(), 10, 138, dateColor);
    headingLeft.SetText("  President insists: ", true, GAME_WIDTH);

    headingRight.Init(state.assets.GetNewspaperFont(), 10, 138 + 15, dateColor);
    headingRight.SetText("  \"No U.S. soldiers in Viet Nam\". ", true, GAME_WIDTH);
}

void Newspaper::LoadClassifiedsData() {
    // Load car metadata from the state's car data
    // We'll get the metadata that was already loaded during initialization
    
    // Access the car metadata from the state's carData
    // The carData should already have the metadata loaded from SrLibDataReadDataFromBuffer
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        int carAssetId = CAR_INDEX[i];  // Get the actual asset ID from CAR_INDEX
        CarMetadata_t metadata = state.carData.GetCarMetadata(carAssetId);
        
        // Only add metadata if it has valid classified data
        if (metadata.classified.carName && strlen(metadata.classified.carName) > 0) {
            carMetadata.push_back(metadata);
        }
    }
    
    // Calculate total pages needed
    totalPages = (carMetadata.size() + adsPerPage - 1) / adsPerPage;
    
    SDL_Log("Loaded %zu classifieds entries, %d pages", carMetadata.size(), totalPages);
}

void Newspaper::InitPageAds() {
    // Ads page 1-x
    SDL_Rect textAreaRect = {64, 0, 192, 190};

    adsArea = Bitmap(textAreaRect.w, textAreaRect.h, 32);
    //SDL_FillRect(adsArea.GetSurface(), NULL, SDL_MapRGBA(adsArea.GetSurface()->format, 200, 200, 200, 255));
    adsArea.UpdateFromSurface();
    adsArea.SetPosition(textAreaRect.x, textAreaRect.y);
    adsArea.SetVisible(false);

    AddDivider(1);

    uint32_t headerColor = SDL_MapRGB(surface->format, 32, 32, 32); // Same as date
    autoPartsHeader.Init(state.assets.GetMainFont(), 68, 12, headerColor);
    autoPartsHeader.SetText(autoParts, false, 184);
    
    usedCarsHeader.Init(state.assets.GetMainFont(), 68, 12, headerColor);
    usedCarsHeader.SetText(usedCars, false, 184);

    // Create dynamic classified ads BEFORE updating adsArea surface
    CreateClassifiedAds();
    CreateUsedCarAds();

    adsArea.UpdateFromSurface();
}

void Newspaper::CreateClassifiedAds() {
    classifiedAds.clear();
    
    // Create labels for each classified ad
    for (size_t i = 0; i < carMetadata.size(); i++) {
        Label adLabel;
        std::string adText = FormatClassifiedAd(carMetadata[i].classified, i);
        
        int yPos = 32 + (i % adsPerPage) * 25; // 25 pixels per ad
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0); // Pure black
        adLabel.Init(state.assets.GetMainFont(), 68, yPos, textColor);
        adLabel.SetText(adText, false, 184);
        adLabel.SetVisible(false); // Will be shown based on current page
        
        classifiedAds.push_back(adLabel);
    }
}

void Newspaper::CreateUsedCarAds() {
    usedCarAds.clear();
    
    // Create labels for used car ads (same data, different formatting)
    for (size_t i = 0; i < carMetadata.size(); i++) {
        Label adLabel;
        std::string adText = FormatUsedCarAd(carMetadata[i], i);
        
        int yPos = 32 + (i % adsPerPage) * 25; // 25 pixels per ad
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0); // Pure black
        adLabel.Init(state.assets.GetMainFont(), 68, yPos, textColor);
        adLabel.SetText(adText, false, 184);
        adLabel.SetVisible(false); // Will be shown based on current page
        
        usedCarAds.push_back(adLabel);
    }
}

std::string Newspaper::FormatClassifiedAd(const ClassifiedEntry_t& entry, int index) {
    std::string result;
    
    // Format: "Year CarName - Description - $Price"
    if (entry.carName && entry.adText) {
        result = std::to_string(entry.year) + " " + std::string(entry.carName);
        
        if (strlen(entry.adText) > 0) {
            result += " - " + std::string(entry.adText);
        }
        
        if (entry.price > 0) {
            result += " - $" + std::to_string(entry.price);
        }
    } else {
        result = "Car #" + std::to_string(index + 1) + " - Details unavailable";
    }
    
    // Truncate if too long
    if (result.length() > 50) {
        result = result.substr(0, 47) + "...";
    }
    
    return result;
}

std::string Newspaper::FormatUsedCarAd(const CarMetadata_t& metadata, int index) {
    std::string result;
    
    // Format: "Year CarName - $Price"
    if (metadata.classified.carName) {
        result = std::to_string(metadata.classified.year) + " " + std::string(metadata.classified.carName);
        
        if (metadata.classified.price > 0) {
            result += " - $" + std::to_string(metadata.classified.price);
        } else {
            result += " - Price on request";
        }
    } else {
        result = "Used Car #" + std::to_string(index + 1) + " - Call for details";
    }
    
    // Truncate if too long
    if (result.length() > 50) {
        result = result.substr(0, 47) + "...";
    }
    
    return result;
}

void Newspaper::UpdatePageContent() {
    // Hide all ads first
    for (auto& ad : classifiedAds) {
        ad.SetVisible(false);
    }
    for (auto& ad : usedCarAds) {
        ad.SetVisible(false);
    }
    
    // Show ads for current page
    int startIndex = currentPage * adsPerPage;
    int endIndex = std::min(startIndex + adsPerPage, (int)carMetadata.size());
    
    for (int i = startIndex; i < endIndex; i++) {
        if (i < (int)classifiedAds.size()) {
            classifiedAds[i].SetVisible(currentPage > 0);
        }
        if (i < (int)usedCarAds.size()) {
            usedCarAds[i].SetVisible(currentPage > 0);
        }
    }
}

void Newspaper::AddDivider(int /* y */) {
    // Crashing here
    // SDL_Rect dividerRect = {4, y, 184, 1};
    // SDL_FillRect(adsArea.GetSurface(), &dividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
    // adsArea.UpdateFromSurface();
}

void Newspaper::Draw() {
    Scene::Draw();
    exit.Draw();
    
    headline.Draw();
    date.Draw();
    headingLeft.Draw();
    headingRight.Draw();

    previous.Draw();
    adsArea.Draw();
    next.Draw();

    autoPartsHeader.Draw();
    usedCarsHeader.Draw();
    
    // Draw classified ads
    for (auto& ad : classifiedAds) {
        ad.Draw();
    }
    
    // Draw used car ads
    for (auto& ad : usedCarAds) {
        ad.Draw();
    }
}

void Newspaper::PushEvent(SDL_Event *event) {
    Scene::PushEvent(event);
}

void Newspaper::Action(ZoneId zone) {
    Scene::Action(zone);
    if (sceneTransition != SCENE_SOLID) {
        return;
    }

    switch (zone) { 
        case ZONE_MAGAZINE_AUTO_PARTS:
            currentPage = 1;
            break;
        case ZONE_MAGAZINE_USED_CARS:
            currentPage = 4;
            break;
        case ZONE_MAGAZINE_BACK_TO_GARAGE:
            state.MoveToScene(SCENE_GARAGE);
            break;
        case ZONE_MAGAZINE_PREVIOUS:
            if (currentPage > 0) {
                currentPage--;
            } else {
                currentPage = totalPages;
            }
            break;
        case ZONE_MAGAZINE_NEXT:
            if (currentPage < totalPages) {
                currentPage++;
            } else {
                currentPage = 0;
            }
            break;
    }

    SDL_Log("Page: %d of %d", currentPage, totalPages);

    Update();
}

void Newspaper::Update() {
    background->SetVisible(currentPage == 0);
    headline.SetVisible(currentPage == 0);
    headingLeft.SetVisible(currentPage == 0);
    headingRight.SetVisible(currentPage == 0);
    previous.SetVisible(currentPage > 0);
    next.SetVisible(currentPage > 0);
    adsArea.SetVisible(currentPage > 0);
    
    autoPartsHeader.SetVisible(currentPage == 1);
    usedCarsHeader.SetVisible(currentPage == 4);

    SetZoneEnabled(ZONE_MAGAZINE_AUTO_PARTS, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_USED_CARS, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_BACK_TO_GARAGE, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_PREVIOUS, currentPage > 0);
    SetZoneEnabled(ZONE_MAGAZINE_NEXT, currentPage > 0);
    
    // Update page content
    UpdatePageContent();
}
