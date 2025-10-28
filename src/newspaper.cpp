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
    
    // Create the large surface with all ads after data is loaded
    CreateAllAdsSurface();

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

    // Create dynamic classified ads BEFORE updating adsArea surface
    CreateClassifiedAds();
    CreateUsedCarAds();
    
    // Create single clickable zone for the ads area
    SDL_Rect adsZoneRect = {64, 0, 192, 190};
    Zone adsZone(ZONE_MAGAZINE_ADS_AREA, const_cast<char*>("Click on an ad to view details"), &adsZoneRect);
    zones.push_back(adsZone);

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
    // Use the new viewport system
    UpdateViewport();
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

    // Draw classified ads
    for (auto& ad : classifiedAds) {
        ad.Draw();
    }
    
    // Draw used car ads
    for (auto& ad : usedCarAds) {
        ad.Draw();
    }
    
    // Draw active dialog
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }
}

void Newspaper::PushEvent(SDL_Event *event) {
    // Handle dialog events first
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return; // Skip normal zone handling when dialog is visible
    }
    
    // Handle mouse wheel scrolling in ads area
    if (event->type == SDL_MOUSEWHEEL && currentPage >= 4) { // Only allow smooth scrolling on used car pages
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        
        // Check if mouse is over the ads area (64, 0, 192, 190)
        if (mouseX >= 64 && mouseX < 256 && mouseY >= 0 && mouseY < 190) {
            int wheelY = event->wheel.y;
            
            if (wheelY > 0) { // Scroll up
                smoothScrollOffset -= 20;
            }
            else if (wheelY < 0) { // Scroll down
                smoothScrollOffset += 20;
            }
            
            // Update the viewport with smooth scrolling
            UpdateViewport();
            return; // Don't process this event further
        }
    }
    
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
            smoothScrollOffset = 0; // Reset smooth scroll when changing sections
            break;
        case ZONE_MAGAZINE_USED_CARS:
            currentPage = 4;
            smoothScrollOffset = 0; // Reset smooth scroll when changing sections
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
            smoothScrollOffset = 0; // Reset smooth scroll when changing pages
            break;
        case ZONE_MAGAZINE_NEXT:
            if (currentPage < totalPages) {
                currentPage++;
            } else {
                currentPage = 0;
            }
            smoothScrollOffset = 0; // Reset smooth scroll when changing pages
            break;
        case ZONE_MAGAZINE_ADS_AREA:
            {
                // Get mouse position and find which ad was clicked
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);
                
                // Convert to ads area coordinates
                int adsAreaX = mouseX - 64; // ads area starts at x=64
                int adsAreaY = mouseY;      // ads area starts at y=0
                
                if (adsAreaX >= 0 && adsAreaX < 192 && adsAreaY >= 0 && adsAreaY < 190) {
                    int adIndex = FindAdAtPosition(adsAreaY);
                    if (adIndex >= 0) {
                        HandleAdClick(adIndex);
                    }
                }
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
    
    // Headers are now part of the large surface

    SetZoneEnabled(ZONE_MAGAZINE_AUTO_PARTS, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_USED_CARS, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_BACK_TO_GARAGE, currentPage == 0);
    SetZoneEnabled(ZONE_MAGAZINE_PREVIOUS, currentPage > 0);
    SetZoneEnabled(ZONE_MAGAZINE_NEXT, currentPage > 0);
    SetZoneEnabled(ZONE_MAGAZINE_ADS_AREA, currentPage > 0); // Only enable ads area zone when on ads pages
    
    // Update page content
    UpdatePageContent();
}

void Newspaper::HandleAdClick(int adIndex) {
    // For now, only handle used car ads (currentPage >= 4)
    if (currentPage < 4) {
        return; // Parts section - no ads yet
    }
    
    // adIndex is the position in the adPositions vector
    if (adIndex >= 0 && adIndex < (int)adPositions.size()) {
        const AdPosition& adPos = adPositions[adIndex];
        if (adPos.isUsedCar && adPos.adIndex >= 0 && adPos.adIndex < (int)carMetadata.size()) {
            selectedCarIndex = adPos.adIndex;
            ShowCarPurchaseDialog(adPos.adIndex);
        }
    }
}

void Newspaper::CreateAllAdsSurface() {
    SDL_Log("CreateAllAdsSurface: Starting with %zu car metadata entries", carMetadata.size());
    
    // Calculate total height needed for all ads
    int totalHeight = 0;
    
    // Add space for PARTS header only (no ads yet)
    totalHeight += 60; // Header (20px) + spacing (40px)
    
    // Add space for used car ads (continuous list, not pages)
    totalHeight += 20; // USED CARS header
    totalHeight += carMetadata.size() * 25; // Rough estimate for all car ads
    
    // Add some padding
    totalHeight += 100;
    
    allAdsSurfaceHeight = totalHeight;
    SDL_Log("CreateAllAdsSurface: Calculated total height: %d", totalHeight);
    
    // Create the large surface
    allAdsSurface = Bitmap(192, totalHeight, 32);
    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (!surface) {
        SDL_Log("ERROR: Failed to create allAdsSurface - GetSurface() returned NULL");
        allAdsSurfaceHeight = 0;
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 255, 255, 255, 255));
    
    // Clear ad positions
    adPositions.clear();
    
    // Get the main font once for all rendering
    SFont_Font* mainFont = state.assets.GetMainFont();
    if (!mainFont) {
        SDL_Log("ERROR: GetMainFont() returned NULL");
        return;
    }
    
    int currentY = 0;
    
    // Draw PARTS header only (no ads yet)
    {
        // Create header label
        
        Label headerLabel;
        uint32_t headerColor = SDL_MapRGB(surface->format, 32, 32, 32);
        headerLabel.Init(mainFont, 0, currentY, headerColor);
        headerLabel.SetText(autoParts, false, 184);
        headerLabel.UpdateFromSurface(); // Make sure the label surface is updated
        
        // Center the header text
        int textWidth = SFont_TextWidth(state.assets.GetMainFont(), autoParts.c_str());
        int centerX = (192 - textWidth) / 2;
        
        // Render header to surface
        SDL_Rect headerRect = {centerX, currentY, 184, 20};
        SDL_BlitSurface(headerLabel.GetSurface(), NULL, allAdsSurface.GetSurface(), &headerRect);
        SDL_Log("Rendered PARTS header at Y=%d", currentY);
        currentY += 20;
        
        // Add some space after the header
        currentY += 40;
    }
    
    // Draw USED CARS header
    {
        // Create header label
        Label headerLabel;
        uint32_t headerColor = SDL_MapRGB(surface->format, 32, 32, 32);
        headerLabel.Init(mainFont, 0, currentY, headerColor);
        headerLabel.SetText(usedCars, false, 184);
        headerLabel.UpdateFromSurface(); // Make sure the label surface is updated
        
        // Center the header text
        int textWidth = SFont_TextWidth(mainFont, usedCars.c_str());
        int centerX = (192 - textWidth) / 2;
        
        // Render header to surface
        SDL_Rect headerRect = {centerX, currentY, 184, 20};
        SDL_BlitSurface(headerLabel.GetSurface(), NULL, surface, &headerRect);
        SDL_Log("Rendered USED CARS header at Y=%d", currentY);
        currentY += 20;
    }
    
    // Draw all used car ads (no page concept, just continuous list)
    {
        // Draw top divider line
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(surface, &topDividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
        currentY += 2; // +1 for top divider + 1 for spacing
        
        for (int i = 0; i < (int)carMetadata.size(); i++) {
            std::string adText = FormatUsedCarAd(carMetadata[i], i);
            
            // Count lines in the ad text to determine height
            int lineCount = 1;
            for (char c : adText) {
                if (c == '\n') lineCount++;
            }
            
            // Calculate more accurate height based on line count
            int adHeight = 8 + (lineCount * 6); // 8px base + 6px per line
            AdPosition adPos;
            adPos.adIndex = i;
            adPos.startY = currentY;
            adPos.endY = currentY + adHeight;
            adPos.isUsedCar = true;
            adPositions.push_back(adPos);
            
            // Create a temporary label to render the text
            Label tempLabel;
            tempLabel.Init(mainFont, 0, currentY, SDL_MapRGB(surface->format, 0, 0, 0));
            tempLabel.SetText(adText, false, 184);
            tempLabel.UpdateFromSurface(); // Make sure the label surface is updated
            
            // Render the label to the large surface
            SDL_Rect destRect = {1, currentY, 184, adHeight};
            SDL_BlitSurface(tempLabel.GetSurface(), NULL, surface, &destRect);
            
            // Calculate next ad position
            currentY += adHeight;
            
            // Draw divider line (except after the last ad)
            if (i < (int)carMetadata.size() - 1) {
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(surface, &dividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
                currentY += 2; // +1 for divider + 1 for spacing
            }
        }
    }
    
    // Update the surface
    allAdsSurface.UpdateFromSurface();
    
    // Debug: Check surface content
    SDL_Surface* debugSurface = allAdsSurface.GetSurface();
    if (debugSurface) {
        SDL_Log("Surface format: %dx%d, %d bpp, %s", 
               debugSurface->w, debugSurface->h, debugSurface->format->BitsPerPixel,
               SDL_GetPixelFormatName(debugSurface->format->format));
        
        // Check if surface has any non-white pixels
        SDL_LockSurface(debugSurface);
        Uint32* pixels = (Uint32*)debugSurface->pixels;
        int nonWhitePixels = 0;
        for (int i = 0; i < debugSurface->w * debugSurface->h; i++) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixels[i], debugSurface->format, &r, &g, &b, &a);
            if (r != 255 || g != 255 || b != 255) {
                nonWhitePixels++;
            }
        }
        SDL_UnlockSurface(debugSurface);
        SDL_Log("Surface has %d non-white pixels out of %d total", nonWhitePixels, debugSurface->w * debugSurface->h);
    }
    
    // Save to file for debugging (after updating the surface)
    SaveAdsSurfaceToFile();
    
    SDL_Log("Created all ads surface: %dx%d", 192, allAdsSurfaceHeight);
    SDL_Log("Total ads rendered: %zu", adPositions.size());
}

void Newspaper::UpdateViewport() {
    // Prevent concurrent viewport updates
    if (isUpdatingViewport) {
        return;
    }
    isUpdatingViewport = true;
    
    // Basic validation
    if (allAdsSurfaceHeight <= 0) {
        isUpdatingViewport = false;
        return;
    }
    
    SDL_Surface* allAdsSurfacePtr = allAdsSurface.GetSurface();
    SDL_Surface* adsAreaSurfacePtr = adsArea.GetSurface();
    if (!allAdsSurfacePtr || !adsAreaSurfacePtr) {
        isUpdatingViewport = false;
        return;
    }
    
    // Calculate viewport position based on current page
    int viewportY = 0;
    
    if (currentPage >= 1 && currentPage <= 3) {
        // PARTS header pages (all show the same header)
        viewportY = 0; // Always start at the beginning (PARTS header)
    } else if (currentPage >= 4) {
        // Used car ads - scroll through the continuous surface
        // Skip PARTS section: header (20px) + spacing (40px)
        int partsSectionHeight = 60; // header + spacing
        int pageScrollOffset = (currentPage - 4) * 190; // 190px per "page" of scrolling
        
        // Calculate base viewport position
        viewportY = partsSectionHeight + pageScrollOffset;
        
        // Add smooth scroll offset (always add it, but clamp the result)
        viewportY += smoothScrollOffset;
        
        // Clamp viewport to prevent scrolling beyond content
        int maxViewportY = allAdsSurfaceHeight - 190; // 190 is viewport height
        if (viewportY > maxViewportY) {
            viewportY = maxViewportY;
            // Adjust smooth scroll offset to match the clamped viewport
            smoothScrollOffset = viewportY - partsSectionHeight - pageScrollOffset;
    }
        if (viewportY < partsSectionHeight) {
            viewportY = partsSectionHeight;
            // Adjust smooth scroll offset to match the clamped viewport
            smoothScrollOffset = 0;
        }
    }
    
    // Clamp viewport to valid bounds
    int maxViewportY = allAdsSurfaceHeight - 190;
    if (viewportY > maxViewportY) viewportY = maxViewportY;
    if (viewportY < 0) viewportY = 0;
    
    // Set the viewport
    adsViewport = {0, viewportY, 192, 190};
    
    // Clear the ads area
    SDL_FillRect(adsAreaSurfacePtr, NULL, SDL_MapRGBA(adsAreaSurfacePtr->format, 255, 255, 255, 255));
    
    // Blit the viewport from the large surface
    if (viewportY < allAdsSurfaceHeight) {
            SDL_Rect srcRect = adsViewport;
            SDL_Rect destRect = {0, 0, 192, 190};
        
        // Check for surface format mismatch (the likely cause of corruption)
        if (allAdsSurfacePtr->format->format != adsAreaSurfacePtr->format->format) {
            // Convert and blit
            SDL_Surface* convertedSurface = SDL_ConvertSurface(allAdsSurfacePtr, adsAreaSurfacePtr->format, 0);
            if (convertedSurface) {
                SDL_BlitSurface(convertedSurface, &srcRect, adsAreaSurfacePtr, &destRect);
                SDL_FreeSurface(convertedSurface);
        }
    } else {
            // Direct blit
            SDL_BlitSurface(allAdsSurfacePtr, &srcRect, adsAreaSurfacePtr, &destRect);
        }
    }
    
    adsArea.UpdateFromSurface();
    
    // Reset the update flag
    isUpdatingViewport = false;
}

void Newspaper::SaveAdsSurfaceToFile() {
    // Save the large surface to a PNG file for debugging using the existing Bitmap method
    SDL_Log("Saving all ads surface to PNG...");
    
    // Debug: Check surface format
    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (surface) {
        SDL_Log("Surface format: %dx%d, %d bpp, %s", 
               surface->w, surface->h, surface->format->BitsPerPixel,
               SDL_GetPixelFormatName(surface->format->format));
    }
    
    allAdsSurface.SaveBitmap(999); // Use a unique index for the ads surface
    SDL_Log("Saved all ads surface to sprite_999.png");
}

int Newspaper::FindAdAtPosition(int mouseY) {
    // Convert mouse Y position to position in the large surface
    int surfaceY = mouseY + adsViewport.y;
    
    // Find which ad contains this Y position
    for (const auto& adPos : adPositions) {
        if (surfaceY >= adPos.startY && surfaceY < adPos.endY) {
            return adPos.adIndex;
        }
    }
    
    return -1; // No ad found at this position
}

void Newspaper::ShowCarPurchaseDialog(int carIndex) {
    if (carIndex < 0 || carIndex >= (int)carMetadata.size()) {
        return;
    }
    
    const CarMetadata_t& metadata = carMetadata[carIndex];
    const char* carName = metadata.classified.carName ? metadata.classified.carName : "Unknown Car";
    int price = metadata.classified.price;
    
    // Dialog dimensions
    const int dialogWidth = 200;
    const int dialogHeight = 60;
    const int dialogX = (GAME_WIDTH - dialogWidth) / 2;
    const int dialogY = (GAME_HEIGHT - dialogHeight) / 2;
    
    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};
    
    // Create dialog text
    std::string dialogText = std::string(carName) + ".\nYou want to buy it for $" + std::to_string(price) + "?";
    
    // Button layout
    const int buttonWidth = 50;
    const int buttonHeight = 14;
    const int buttonSpacing = 4;
    const int buttonY = dialogY + dialogHeight - buttonHeight - 4;
    
    SDL_Rect seeItButtonRect = {dialogX + 4, buttonY, buttonWidth, buttonHeight};
    SDL_Rect yeahButtonRect = {dialogX + 4 + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetButtonRect = {dialogX + 4 + 2 * (buttonWidth + buttonSpacing), buttonY, buttonWidth, buttonHeight};
    
    ButtonConfig seeItButton;
    seeItButton.bounds = seeItButtonRect;
    seeItButton.text = "See it";
    seeItButton.onClick = nullptr;
    seeItButton.defaultChoice = false;
    seeItButton.index = 0;
    
    ButtonConfig yeahButton;
    yeahButton.bounds = yeahButtonRect;
    yeahButton.text = "Yeah";
    yeahButton.onClick = nullptr;
    yeahButton.defaultChoice = true; // Default button
    yeahButton.index = 1;
    
    ButtonConfig forgetButton;
    forgetButton.bounds = forgetButtonRect;
    forgetButton.text = "Forget it";
    forgetButton.onClick = nullptr;
    forgetButton.defaultChoice = false;
    forgetButton.index = 2;
    
    std::vector<ButtonConfig> buttons = {seeItButton, yeahButton, forgetButton};
    
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{dialogX + 3, dialogY + 3, dialogWidth - 6, 30}, dialogText, {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    
    DialogConfig dialogConfig;
    dialogConfig.bounds = dialogRect;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.texts = texts;
    dialogConfig.buttons = buttons;
    dialogConfig.contentAreas = std::vector<SDL_Rect>{dialogRect};
    dialogConfig.font = state.assets.GetMainFont();
    dialogConfig.onButtonPressed = [this, carIndex](int buttonIndex) {
        switch (buttonIndex) {
            case 0: // See it
                ShowCarViewDialog(carIndex);
                break;
            case 1: // Yeah
                // TODO: Implement purchase logic
                SDL_Log("Player wants to buy car %d", carIndex);
                carPurchaseDialog->Hide();
                SetActiveDialog(nullptr);
                break;
            case 2: // Forget it
                carPurchaseDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    
    carPurchaseDialog = std::make_unique<Dialog>(dialogConfig);
    carPurchaseDialog->Show();
    SetActiveDialog(carPurchaseDialog.get());
}

void Newspaper::ShowCarViewDialog(int carIndex) {
    if (carIndex < 0 || carIndex >= (int)carMetadata.size()) {
        return;
    }
    
    // Hide the purchase dialog first
    if (carPurchaseDialog) {
        carPurchaseDialog->Hide();
    }
    
    // Create car preview bitmap
    CreateCarPreviewBitmap(carIndex);
    
    const CarMetadata_t& metadata = carMetadata[carIndex];
    const char* carName = metadata.classified.carName ? metadata.classified.carName : "Unknown Car";
    int price = metadata.classified.price;
    
    // Dialog dimensions (larger to accommodate car bitmap)
    const int dialogWidth = 250;
    const int dialogHeight = 120;
    const int dialogX = (GAME_WIDTH - dialogWidth) / 2;
    const int dialogY = (GAME_HEIGHT - dialogHeight) / 2;
    
    SDL_Rect dialogRect = {dialogX, dialogY, dialogWidth, dialogHeight};
    
    // Create dialog text
    std::string dialogText = std::string(carName) + ".\nYou want to buy it for $" + std::to_string(price) + "?";
    
    // Button layout
    const int buttonWidth = 50;
    const int buttonHeight = 14;
    const int buttonSpacing = 4;
    const int buttonY = dialogY + dialogHeight - buttonHeight - 4;
    
    SDL_Rect seeItButtonRect = {dialogX + 4, buttonY, buttonWidth, buttonHeight};
    SDL_Rect yeahButtonRect = {dialogX + 4 + buttonWidth + buttonSpacing, buttonY, buttonWidth, buttonHeight};
    SDL_Rect forgetButtonRect = {dialogX + 4 + 2 * (buttonWidth + buttonSpacing), buttonY, buttonWidth, buttonHeight};
    
    ButtonConfig seeItButton;
    seeItButton.bounds = seeItButtonRect;
    seeItButton.text = "See it";
    seeItButton.onClick = nullptr;
    seeItButton.defaultChoice = false;
    seeItButton.index = 0;
    
    ButtonConfig yeahButton;
    yeahButton.bounds = yeahButtonRect;
    yeahButton.text = "Yeah";
    yeahButton.onClick = nullptr;
    yeahButton.defaultChoice = true; // Default button
    yeahButton.index = 1;
    
    ButtonConfig forgetButton;
    forgetButton.bounds = forgetButtonRect;
    forgetButton.text = "Forget it";
    forgetButton.onClick = nullptr;
    forgetButton.defaultChoice = false;
    forgetButton.index = 2;
    
    std::vector<ButtonConfig> buttons = {seeItButton, yeahButton, forgetButton};
    
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{dialogX + 3, dialogY + 3, dialogWidth - 6, 30}, dialogText, {0xFF, 0xFF, 0xFF, 0xFF}, true}
    };
    
    // Add car bitmap to dialog
    std::map<std::string, DialogBitmap> bitmaps;
    if (carPreviewBitmap) {
        SDL_Rect carRect = {dialogX + 10, dialogY + 35, 100, 60}; // Position for car bitmap
        DialogBitmap carBitmap;
        carBitmap.bitmap = carPreviewBitmap.get();
        carBitmap.rect = carRect;
        carBitmap.zOrder = 0;
        bitmaps["car_preview"] = carBitmap;
    }
    
    DialogConfig dialogConfig;
    dialogConfig.bounds = dialogRect;
    dialogConfig.backgroundColor = {0, 208, 0, 255};
    dialogConfig.borderColor = {0, 111, 239, 255};
    dialogConfig.texts = texts;
    dialogConfig.buttons = buttons;
    dialogConfig.contentAreas = std::vector<SDL_Rect>{dialogRect};
    dialogConfig.bitmaps = bitmaps;
    dialogConfig.font = state.assets.GetMainFont();
    dialogConfig.onButtonPressed = [this, carIndex](int buttonIndex) {
        switch (buttonIndex) {
            case 0: // See it (already showing)
                // Do nothing, already showing the car
                break;
            case 1: // Yeah
                // TODO: Implement purchase logic
                SDL_Log("Player wants to buy car %d", carIndex);
                carViewDialog->Hide();
                SetActiveDialog(nullptr);
                break;
            case 2: // Forget it
                carViewDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };
    
    carViewDialog = std::make_unique<Dialog>(dialogConfig);
    carViewDialog->Show();
    SetActiveDialog(carViewDialog.get());
}

void Newspaper::CreateCarPreviewBitmap(int carIndex) {
    if (carIndex < 0 || carIndex >= (int)carMetadata.size()) {
        return;
    }
    
    // For now, create a simple placeholder bitmap
    // TODO: Integrate with the actual car rendering system using carIndex
    // The carIndex corresponds to the CAR_INDEX array in libp.h
    
    // Create a simple colored rectangle as placeholder
    carPreviewBitmap = std::make_unique<Bitmap>(100, 60, 32);
    SDL_FillRect(carPreviewBitmap->GetSurface(), NULL, SDL_MapRGBA(carPreviewBitmap->GetSurface()->format, 100, 100, 200, 255));
    carPreviewBitmap->UpdateFromSurface();
    
    SDL_Log("Created car preview bitmap for car %d", carIndex);
}
