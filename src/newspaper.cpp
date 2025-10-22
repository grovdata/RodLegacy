#include "newspaper.h"
#include "state.h"
#include "SFont.h"
#include <sstream>

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
            std::string adText = metadata.classified.adText ? metadata.classified.adText : "No description";
            SDL_Log("Classified %d: %s - %s - $%d", i, metadata.classified.carName, 
                    adText.c_str(), metadata.classified.price);
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
    SDL_FillRect(adsArea.GetSurface(), NULL, SDL_MapRGBA(adsArea.GetSurface()->format, 255, 255, 255, 255));
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
    
    // Create 6 label slots (6 ads per page, variable height)
    // We'll update their text content and position when the page changes
    for (int i = 0; i < adsPerPage; i++) {
        Label adLabel;
        // Initial position, will be updated based on content
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0); // Pure black
        adLabel.Init(state.assets.GetMainFont(), 68, 32, textColor);
        adLabel.SetVisible(false); // Will be shown based on current page
        
        classifiedAds.push_back(adLabel);
    }
}

void Newspaper::CreateUsedCarAds() {
    usedCarAds.clear();
    
    // Create 7 label slots (6.5 ads per page, variable height)
    // We'll update their text content and position when the page changes
    for (int i = 0; i < adsPerPage + 1; i++) {
        Label adLabel;
        // Initial position, will be updated based on content
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0); // Pure black
        adLabel.Init(state.assets.GetMainFont(), 68, 32, textColor);
        adLabel.SetVisible(false); // Will be shown based on current page
        
        usedCarAds.push_back(adLabel);
    }
}

std::string Newspaper::WordWrap(const std::string& text, int maxWidthPixels) {
    SFont_Font* font = state.assets.GetMainFont();
    if (!font) {
        return text; // Fallback if font not available
    }
    
    // Check if text already fits
    if (SFont_TextWidth(font, text.c_str()) <= maxWidthPixels) {
        return text;
    }
    
    std::string result;
    std::string currentLine;
    std::istringstream iss(text);
    std::string word;
    
    while (iss >> word) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        
        if (SFont_TextWidth(font, testLine.c_str()) <= maxWidthPixels) {
            currentLine = testLine;
            
            // If the word ends with a period, consider breaking here for better readability
            if (word.back() == '.' && SFont_TextWidth(font, testLine.c_str()) > maxWidthPixels * 0.7) {
                // If we're more than 70% full and just finished a sentence, break here
                if (!result.empty()) {
                    result += "\n";
                }
                result += currentLine;
                currentLine.clear();
            }
        } else {
            if (!currentLine.empty()) {
                if (!result.empty()) {
                    result += "\n";
                }
                result += currentLine;
            }
            currentLine = word;
        }
    }
    
    if (!currentLine.empty()) {
        if (!result.empty()) {
            result += "\n";
        }
        result += currentLine;
    }
    
    return result;
}

std::string Newspaper::FormatClassifiedAd(const ClassifiedEntry_t& entry, int index) {
    std::string result;
    
    // Format: "CarName AdText $Price"
    if (entry.carName && entry.adText) {
        result = std::string(entry.carName);
        
        if (strlen(entry.adText) > 0) {
            result += " " + std::string(entry.adText);
        }
        
        if (entry.price > 0) {
            result += " $" + std::to_string(entry.price);
        }
        
        // Apply word wrapping to each line (max 180 pixels per line)
        std::string wrappedResult;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            if (!wrappedResult.empty()) {
                wrappedResult += "\n";
            }
            wrappedResult += WordWrap(line, 180);
        }
        result = wrappedResult;
    } else {
        result = "Car #" + std::to_string(index + 1) + "\nDetails unavailable";
    }
    
    return result;
}

std::string Newspaper::FormatUsedCarAd(const CarMetadata_t& metadata, int index) {
    std::string result;
    
    // Format: "CarName AdText $Price"
    if (metadata.classified.carName && metadata.classified.adText) {
        result = std::string(metadata.classified.carName);
        
        if (strlen(metadata.classified.adText) > 0) {
            result += " " + std::string(metadata.classified.adText);
        }
        
        if (metadata.classified.price > 0) {
            result += " $" + std::to_string(metadata.classified.price);
        } else {
            result += " Price on request";
        }
        
        // Apply word wrapping to each line (max 180 pixels per line)
        std::string wrappedResult;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            if (!wrappedResult.empty()) {
                wrappedResult += "\n";
            }
            wrappedResult += WordWrap(line, 180);
        }
        result = wrappedResult;
    } else {
        result = "Used Car #" + std::to_string(index + 1) + "\nCall for details";
    }
    
    return result;
}

void Newspaper::UpdatePageContent() {
    // Clear the ads area first
    SDL_FillRect(adsArea.GetSurface(), NULL, SDL_MapRGBA(adsArea.GetSurface()->format, 255, 255, 255, 255));
    
    // Clear existing ad zones
    for (int i = 0; i < 6; i++) {
        SetZoneEnabled((ZoneId)(ZONE_MAGAZINE_AD_0 + i), false);
    }
    
    // Hide all ads first
    for (auto& ad : classifiedAds) {
        ad.SetVisible(false);
    }
    for (auto& ad : usedCarAds) {
        ad.SetVisible(false);
    }
    
    // Determine which page range we're in
    // Pages 1-3 are for classifieds (auto parts)
    // Pages 4+ are for used cars
    if (currentPage >= 1 && currentPage <= 3) {
        // Show classified ads for this page
        int pageOffset = currentPage - 1;  // Convert to 0-based page offset
        int startIndex = pageOffset * adsPerPage;
        int endIndex = std::min(startIndex + adsPerPage, (int)carMetadata.size());
        
        int currentY = 32; // Start position for first ad
        
        // Draw top divider line
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(adsArea.GetSurface(), &topDividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
        currentY += 2; // +1 for top divider + 1 for spacing
        
        // Update text and show labels for this page
        for (int i = startIndex; i < endIndex; i++) {
            int labelIndex = i - startIndex;  // 0-2 for the label slot
            std::string adText = FormatClassifiedAd(carMetadata[i].classified, i);
            
            // Count lines in the ad text to determine height
            int lineCount = 1;
            for (char c : adText) {
                if (c == '\n') lineCount++;
            }
            
            // Set position and text
            classifiedAds[labelIndex].SetPosition(68, currentY);
            classifiedAds[labelIndex].SetText(adText, false, 184);
            classifiedAds[labelIndex].SetVisible(true);
            
            // Create clickable zone for this ad
            SDL_Rect adZoneRect = {68, currentY, 184, (lineCount == 2) ? 20 : 29};
            Zone adZone((ZoneId)(ZONE_MAGAZINE_AD_0 + labelIndex), 
                       const_cast<char*>("Click to view ad details"), &adZoneRect);
            zones.push_back(adZone);
            
            // Calculate next ad position: current height + 1 pixel divider
            int adHeight = (lineCount == 2) ? 20 : 29; // 2 lines = 20px, 3+ lines = 29px
            currentY += adHeight;
            
            // Draw divider line (except after the last ad)
            if (i < endIndex - 1) {
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(adsArea.GetSurface(), &dividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
                currentY += 2; // +1 for divider + 1 for spacing
            }
        }
    } else if (currentPage >= 4) {
        // Show used car ads for this page (6.5 ads per page)
        int pageOffset = currentPage - 4;  // Convert to 0-based page offset for used cars
        int startIndex = pageOffset * adsPerPage;
        int adsOnThisPage = (currentPage == 4) ? 7 : adsPerPage; // First used car page shows 6.5 ads
        int endIndex = std::min(startIndex + adsOnThisPage, (int)carMetadata.size());
        
        int currentY = 32; // Start position for first ad
        
        // Draw top divider line
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(adsArea.GetSurface(), &topDividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
        currentY += 2; // +1 for top divider + 1 for spacing
        
        // Update text and show labels for this page
        for (int i = startIndex; i < endIndex; i++) {
            int labelIndex = i - startIndex;  // 0-6 for the label slot
            std::string adText = FormatUsedCarAd(carMetadata[i], i);
            
            // Count lines in the ad text to determine height
            int lineCount = 1;
            for (char c : adText) {
                if (c == '\n') lineCount++;
            }
            
            // Set position and text
            usedCarAds[labelIndex].SetPosition(68, currentY);
            usedCarAds[labelIndex].SetText(adText, false, 184);
            usedCarAds[labelIndex].SetVisible(true);
            
            // Create clickable zone for this ad
            SDL_Rect adZoneRect = {68, currentY, 184, (lineCount == 2) ? 20 : 29};
            Zone adZone((ZoneId)(ZONE_MAGAZINE_AD_0 + labelIndex), 
                       const_cast<char*>("Click to view car details"), &adZoneRect);
            zones.push_back(adZone);
            
            // Calculate next ad position: current height + 1 pixel divider
            int adHeight = (lineCount == 2) ? 20 : 29; // 2 lines = 20px, 3+ lines = 29px
            currentY += adHeight;
            
            // Draw divider line (except after the last ad)
            if (i < endIndex - 1) {
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(adsArea.GetSurface(), &dividerRect, SDL_MapRGBA(adsArea.GetSurface()->format, 0, 0, 0, 255));
                currentY += 2; // +1 for divider + 1 for spacing
            }
        }
    }
    
    // Update the ads area surface
    adsArea.UpdateFromSurface();
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
        case ZONE_MAGAZINE_AD_0:
        case ZONE_MAGAZINE_AD_1:
        case ZONE_MAGAZINE_AD_2:
        case ZONE_MAGAZINE_AD_3:
        case ZONE_MAGAZINE_AD_4:
        case ZONE_MAGAZINE_AD_5:
            {
                int adIndex = zone - ZONE_MAGAZINE_AD_0;
                HandleAdClick(adIndex);
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
    date.SetVisible(currentPage == 0);
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

void Newspaper::HandleAdClick(int adIndex) {
    // Calculate the actual ad index based on current page
    int actualAdIndex;
    
    if (currentPage >= 1 && currentPage <= 3) {
        // Classified ads (auto parts)
        int pageOffset = currentPage - 1;
        actualAdIndex = pageOffset * adsPerPage + adIndex;
    } else if (currentPage >= 4) {
        // Used car ads
        int pageOffset = currentPage - 4;
        actualAdIndex = pageOffset * adsPerPage + adIndex;
    } else {
        return; // Invalid page
    }
    
    // Check bounds
    if (actualAdIndex >= 0 && actualAdIndex < (int)carMetadata.size()) {
        const CarMetadata_t& metadata = carMetadata[actualAdIndex];
        
        if (currentPage >= 1 && currentPage <= 3) {
            // Classified ad clicked
            SDL_Log("Classified ad clicked: %s - %s - $%d", 
                   metadata.classified.carName ? metadata.classified.carName : "Unknown",
                   metadata.classified.adText ? metadata.classified.adText : "No description",
                   metadata.classified.price);
        } else {
            // Used car ad clicked
            SDL_Log("Used car ad clicked: %s - %s - $%d", 
                   metadata.classified.carName ? metadata.classified.carName : "Unknown",
                   metadata.classified.adText ? metadata.classified.adText : "No description",
                   metadata.classified.price);
        }
        
        // TODO: Add actual functionality like showing detailed view or purchasing
    }
}
