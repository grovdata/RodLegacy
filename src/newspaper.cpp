#include "log.h"
#include "newspaper.h"
#include <sstream>
#include "state.h"
#include "car.h"
#include "animated_bitmap.h"
#include "carbitmap.h"
#include "parsers/carcolor.h"
#include "SFont.h"
#include "scene_layout.h"
#include "embedded_resources.h"
#include "ui/bubble.h"

const std::string autoParts = "------------ PARTS ------------";
const std::string usedCars = "---------- USED CARS ----------";

Newspaper::Newspaper() : Scene((Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU}) {
    currentPage = 0;

    zones = SceneLayout::Load(embedded_newspaper_toml);

    GetZone("auto_parts").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        currentPage = 1;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };
    GetZone("used_cars").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        currentPage = 4;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };
    auto backToGarage = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        state.MoveToScene(SCENE_GARAGE);
    };
    GetZone("back_to_garage").onClick = backToGarage;
    GetZone("back_to_garage2").onClick = backToGarage;
    // Page 0 is the front page, pages 1-3 are the fixed "parts" pages, and pages
    // 4..(3+totalPages) are the used-car ad pages (totalPages counts only those).
    GetZone("previous").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        int lastPage = 3 + totalPages;
        if (currentPage > 0) currentPage--; else currentPage = lastPage;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };
    GetZone("next").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        int lastPage = 3 + totalPages;
        if (currentPage < lastPage) currentPage++; else currentPage = 0;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };

    auto cantAffordBubble = std::make_unique<Bubble>("  No go, hot shot.", BUBBLE_LEFT, 90, 95, true);
    cantAffordBubble->SetVisible(false);
    bubbles.push_back(std::move(cantAffordBubble));
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

}

void Newspaper::InitPageOne() {
    previous = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    previous.SetPosition(0, 0);
    SDL_Rect previousRect = {0, 0, 64, 190};
    previous.Crop(previousRect);
    previous.SetDimensions(64, 190);
    previous.UpdateFromSurface();
    previous.SetVisible(false);

    next = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT});
    next.SetPosition(GAME_WIDTH - 64, 0);
    SDL_Rect nextRect = {64, 0, 64, 190};
    next.Crop(nextRect);
    next.SetDimensions(64, 190);
    next.UpdateFromSurface();
    next.SetVisible(false);

    // Set headline based on date?
    AmigaLibAsset headlineAsset = NEWSPAPER_NO_SOLDIERS_IN_VIETNAM;
    headline = state.GetAssets().GetBitmapAsset({LibAssetRegular, SrLibFileAmigaLibP, headlineAsset});
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
    date.Init(state.GetAssets().GetMainFont(), 0, 120, dateColor);
    lastDateText = state.GetGameDateString();
    date.SetText("  " + lastDateText + "  ", true, GAME_WIDTH);

    headingLeft.Init(state.GetAssets().GetNewspaperFont(), 10, 138, dateColor);
    headingLeft.SetText("  President insists: ", true, GAME_WIDTH);

    headingRight.Init(state.GetAssets().GetNewspaperFont(), 10, 138 + 15, dateColor);
    headingRight.SetText("  \"No U.S. soldiers in Viet Nam\". ", true, GAME_WIDTH);
}

void Newspaper::LoadClassifiedsData() {
    // Classified cars are already generated in State::Init
    // Just load the metadata and asset IDs to match
    // Walks the merged car-spec table in the same order and with the same skip
    // condition State::Init used, so this list stays index-aligned with
    // state.classifiedCars - the buy handler indexes that by list position.
    const std::vector<int>& specModelIndex = state.GetCarSpecModelIndex();
    for (int spec = 0; spec < static_cast<int>(specModelIndex.size()); spec++) {
        int carAssetId = specModelIndex[spec];
        if (carAssetId < 0) continue;  // The King's slot, not for sale
        CarMetadata_t metadata = state.carData.GetCarMetadata(carAssetId);

        if (metadata.classified.carName && strlen(metadata.classified.carName) > 0) {
            std::string adText = metadata.classified.adText ? metadata.classified.adText : "No description";
            LOG_INFO("Classified %d: %s - %s - $%d", spec, metadata.classified.carName,
                    adText.c_str(), metadata.classified.price);
            carMetadata.push_back(metadata);
            carAssetIds.push_back(carAssetId);
        }
    }

    totalPages = (carMetadata.size() + adsPerPage - 1) / adsPerPage;

    LOG_INFO("Loaded %zu classifieds entries, %d pages", carMetadata.size(), totalPages);
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
    Zone adsZone("ads_area", "Click on an ad to view details", adsZoneRect);
    adsZone.onClick = [this] {
        int adsAreaX = lastClickX - 64;
        int adsAreaY = lastClickY;
        if (adsAreaX >= 0 && adsAreaX < 192 && adsAreaY >= 0 && adsAreaY < 190) {
            int adIndex = FindAdAtPosition(adsAreaY);
            if (adIndex >= 0) HandleAdClick(adIndex);
        }
    };
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
        adLabel.Init(state.GetAssets().GetMainFont(), 68, 32, textColor);
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
        adLabel.Init(state.GetAssets().GetMainFont(), 68, 32, textColor);
        adLabel.SetVisible(false); // Will be shown based on current page

        usedCarAds.push_back(adLabel);
    }
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

void Newspaper::DrawScene() {
    Scene::DrawScene();
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

    // Draw bubbles last so they appear on top of dialogs
    for (auto& b : bubbles) {
        b->Draw();
    }
}

bool Newspaper::PushEvent(SDL_Event *event) {
    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->HandleEvent(event);
        return true;
    }

    if (event->type == SDL_MOUSEWHEEL && currentPage >= 4) {
        int mouseX = event->wheel.mouseX;
        int mouseY = event->wheel.mouseY;
        if (mouseX >= 64 && mouseX < 256 && mouseY >= 0 && mouseY < 190) {
            if (event->wheel.y > 0) {
                smoothScrollOffset -= 20;
            } else if (event->wheel.y < 0) {
                smoothScrollOffset += 20;
            }
            UpdateViewport();
            return true;
        }
    }
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        lastClickX = event->button.x;
        lastClickY = event->button.y;
    }
    return Scene::PushEvent(event);
}


void Newspaper::Update() {
    // Note: this overload is unrelated to the real per-frame Update(Uint64)
    // below -- it's only called from a handful of explicit spots (page
    // turns, ad clicks, Init) to refresh page visibility/content.

    background->SetVisible(currentPage == 0);
    headline.SetVisible(currentPage == 0);
    headingLeft.SetVisible(currentPage == 0);
    headingRight.SetVisible(currentPage == 0);
    date.SetVisible(currentPage == 0);
    previous.SetVisible(currentPage > 0);
    next.SetVisible(currentPage > 0);
    adsArea.SetVisible(currentPage > 0);

    // Headers are now part of the large surface

    SetZoneEnabled("auto_parts", currentPage == 0);
    SetZoneEnabled("used_cars", currentPage == 0);
    SetZoneEnabled("back_to_garage", currentPage == 0);
    SetZoneEnabled("previous", currentPage > 0);
    SetZoneEnabled("next", currentPage > 0);
    SetZoneEnabled("ads_area", currentPage > 0);

    // Update page content
    UpdatePageContent();
}

void Newspaper::Update(Uint64 deltaTime) {
    Scene::Update(deltaTime);  // fade handling, otherwise lost by overriding this

    // Catches any change to the game date (loading a save, or normal
    // time-advancing actions). Cheap: a string compare every frame: the
    // actual re-render (date.SetText, which rebuilds a surface) only runs
    // when the text actually changed.
    std::string currentDateText = state.GetGameDateString();
    if (currentDateText != lastDateText) {
        lastDateText = currentDateText;
        date.SetText("  " + lastDateText + "  ", true, GAME_WIDTH);
    }
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
            // Don't open dialog for already-sold cars
            if (!state.GetClassifiedCars()[adPos.adIndex]) {
                return;
            }
            selectedCarIndex = adPos.adIndex;
            ShowCarPurchaseDialog(adPos.adIndex);
        }
    }
}

void Newspaper::CreateAllAdsSurface() {
    LOG_INFO("CreateAllAdsSurface: Starting with %zu car metadata entries", carMetadata.size());

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
    LOG_INFO("CreateAllAdsSurface: Calculated total height: %d", totalHeight);

    // Create the large surface
    allAdsSurface = Bitmap(192, totalHeight, 32);
    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (!surface) {
        LOG_INFO("ERROR: Failed to create allAdsSurface - GetSurface() returned NULL");
        allAdsSurfaceHeight = 0;
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 255, 255, 255, 255));

    // Clear ad positions
    adPositions.clear();

    // Get the main font once for all rendering
    SFont_Font* mainFont = state.GetAssets().GetMainFont();
    if (!mainFont) {
        LOG_INFO("ERROR: GetMainFont() returned NULL");
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
        int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), autoParts.c_str());
        int centerX = (192 - textWidth) / 2;

        // Render header to surface
        SDL_Rect headerRect = {centerX, currentY, 184, 20};
        SDL_BlitSurface(headerLabel.GetSurface(), NULL, allAdsSurface.GetSurface(), &headerRect);
        LOG_INFO("Rendered PARTS header at Y=%d", currentY);
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
        LOG_INFO("Rendered USED CARS header at Y=%d", currentY);
        currentY += 20;
    }

    // Draw all used car ads (no page concept, just continuous list)
    //
    // Original DOS layout (reversed from FUN_1000_09cb in SR.EXE):
    //   - Car name is rendered at left column x=LEFT_COL, current row y.
    //   - Lines from data that start with '+' (stored here as '\t'-prefixed) are
    //     rendered at RIGHT_COL on the SAME row as the car name (no y advance).
    //     At most one feature line per entry in the actual game data.
    //   - All other lines advance y by lineHeight and render at LEFT_COL.
    //   - The price string (e.g. "$1450") is stored as the last regular line in adText.
    {
        const int LEFT_COL = 1;
        // Minimum right column x. For car names wider than this, the feature
        // text is placed just after the name end + a small gap instead.
        const int RIGHT_COL_MIN = 96;
        const int RIGHT_COL_GAP = 4;
        const int lineHeight = SFont_TextHeight(mainFont);

        SDL_SetSurfaceColorMod(mainFont->Surface, 0, 0, 0);

        // Draw top divider line (1px top margin above first ad)
        currentY += 1;
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(surface, &topDividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
        currentY += 2;

        for (int i = 0; i < (int)carMetadata.size(); i++) {
            const ClassifiedEntry_t& entry = carMetadata[i].classified;
            const char* carName = entry.carName ? entry.carName : "";
            int adStartY = currentY;

            // Car name: left column, current row
            SFont_Write(surface, mainFont, LEFT_COL, currentY, carName);

            // Right column: at least RIGHT_COL_MIN, but pushed right if the car
            // name extends past that to avoid overlap.
            int carNameWidth = SFont_TextWidth(mainFont, carName);
            int featureX = std::max(RIGHT_COL_MIN, LEFT_COL + carNameWidth + RIGHT_COL_GAP);

            // Render adText lines.
            // '\t'-prefixed lines go in the right column at the same y as car name.
            // All other lines advance y and render at left column.
            if (entry.adText && entry.adText[0]) {
                std::istringstream ss(entry.adText);
                std::string line;
                while (std::getline(ss, line)) {
                    if (!line.empty() && line[0] == '\t') {
                        SFont_Write(surface, mainFont, featureX, currentY, line.c_str() + 1);
                    } else {
                        currentY += lineHeight;
                        SFont_Write(surface, mainFont, LEFT_COL, currentY, line.c_str());
                    }
                }
            }

            int adHeight = (currentY - adStartY) + lineHeight;
            AdPosition adPos;
            adPos.adIndex = i;
            adPos.startY = adStartY;
            adPos.endY = adStartY + adHeight;
            adPos.isUsedCar = true;
            adPositions.push_back(adPos);

            currentY += lineHeight;  // advance past last rendered row

            // Draw divider line (4px bottom margin below each ad, except the last)
            if (i < (int)carMetadata.size() - 1) {
                currentY += 4;
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(surface, &dividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
                currentY += 2;
            }
        }

        SDL_SetSurfaceColorMod(mainFont->Surface, 255, 255, 255);
    }

    // Update the surface
    allAdsSurface.UpdateFromSurface();

    // Debug: Check surface content
    SDL_Surface* debugSurface = allAdsSurface.GetSurface();
    if (debugSurface) {
        LOG_INFO("Surface format: %dx%d, %d bpp, %s",
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
        LOG_INFO("Surface has %d non-white pixels out of %d total", nonWhitePixels, debugSurface->w * debugSurface->h);
    }

    // Save to file for debugging (after updating the surface)
    SaveAdsSurfaceToFile();

    LOG_INFO("Created all ads surface: %dx%d", 192, allAdsSurfaceHeight);
    LOG_INFO("Total ads rendered: %zu", adPositions.size());
}

void Newspaper::UpdateViewport() {
    // Basic validation
    if (allAdsSurfaceHeight <= 0) {
        return;
    }

    SDL_Surface* allAdsSurfacePtr = allAdsSurface.GetSurface();
    SDL_Surface* adsAreaSurfacePtr = adsArea.GetSurface();
    if (!allAdsSurfacePtr || !adsAreaSurfacePtr) {
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
            smoothScrollOffset = viewportY - partsSectionHeight - pageScrollOffset;
        }
        if (viewportY < partsSectionHeight) {
            viewportY = partsSectionHeight;
            smoothScrollOffset = 0;
        }
    }

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
}

void Newspaper::SaveAdsSurfaceToFile() {
    // Save the large surface to a PNG file for debugging using the existing Bitmap method
    LOG_INFO("Saving all ads surface to PNG...");

    // Debug: Check surface format
    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (surface) {
        LOG_INFO("Surface format: %dx%d, %d bpp, %s",
               surface->w, surface->h, surface->format->BitsPerPixel,
               SDL_GetPixelFormatName(surface->format->format));
    }

    allAdsSurface.SaveBitmap(999); // Use a unique index for the ads surface
    LOG_INFO("Saved all ads surface to sprite_999.png");
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

    SDL_Rect dialogRect = {34, 97, 252, 46};

    SDL_Rect seeItRect = {50, 123, 66, 16};
    SDL_Rect yeahRect = {127, 123, 66, 16};
    SDL_Rect forgetItRect = {206, 123, 66, 16};

    std::vector<ButtonConfig> buttons = {
        {seeItRect, "See it", nullptr, false, 0},
        {yeahRect, "Yeah", nullptr, true, 1},
        {forgetItRect, "Forget it", nullptr, false, 2}
    };

    std::string priceText = "You want to buy it for $" + std::to_string(price) + " ?";

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{38, 101, 248, 10}, std::string(carName), {0x00, 0x00, 0x00, 0xFF}, true},
        DialogTextConfig{SDL_Rect{38, 111, 248, 10}, priceText, {0x00, 0x00, 0x00, 0xFF}, true}
    };

    DialogConfig dialogConfig;
    dialogConfig.bounds = dialogRect;
    dialogConfig.backgroundColor = {0x90, 0x90, 0x90, 0xFF};
    dialogConfig.borderColor = {0xFF, 0xFF, 0xFF, 0xFF};
    dialogConfig.texts = texts;
    dialogConfig.buttons = buttons;
    dialogConfig.contentAreas = std::vector<SDL_Rect>{dialogRect};
    dialogConfig.font = state.GetAssets().GetMainFont();
    dialogConfig.onButtonPressed = [this, carIndex, price](int buttonIndex) {
        switch (buttonIndex) {
            case 0: // See it
                ShowCarViewDialog(carIndex);
                break;
            case 1: // Yeah - buy the car
                if (state.GetBankroll() < price) {
                    LOG_INFO("Not enough money to buy car %d (need $%d, have $%d)",
                            carIndex, price, state.GetBankroll());
                    int short_ = price - state.GetBankroll();
                    bubbles[0]->SetText("  No go, hot shot. You're $" + std::to_string(short_) + " short.");
                    bubbles[0]->SetVisible(true);
                } else if (!state.GetClassifiedCars()[carIndex]) {
                    LOG_INFO("Car %d already sold", carIndex);
                } else {
                    state.SetBankroll(state.GetBankroll() - price);
                    Car* boughtCar = state.GetClassifiedCars()[carIndex];
                    state.AddOwnedCar(boughtCar);
                    state.SetCurrentCar(boughtCar);
                    state.GetClassifiedCars()[carIndex] = nullptr;
                    LOG_INFO("Bought car %d (%s) for $%d, bankroll now $%d, owned cars: %zu",
                            carIndex, boughtCar->GetModel().name.c_str(),
                            price, state.GetBankroll(),
                            state.GetOwnedCars().size());
                    carPurchaseDialog->Hide();
                    SetActiveDialog(nullptr);
                    state.MoveToScene(SCENE_GARAGE);
                    return;
                }
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

    if (carPurchaseDialog) {
        carPurchaseDialog->Hide();
    }

    const CarMetadata_t& metadata = carMetadata[carIndex];
    const char* carName = metadata.classified.carName ? metadata.classified.carName : "Unknown Car";

    Car* car = state.GetClassifiedCars()[carIndex];
    if (!car) {
        return; // Already sold
    }

    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    std::vector<uint16_t> palette = GetStandardCarColor(car->GetColor());

    // Load body with car's color palette. carAssetIds holds the pool key,
    // which for a Street Rod SE car is not a lib.p asset - go through the
    // model so the right lib is used.
    Bitmap* carBodyBitmap = LoadCarModelSprite(model, model.sprites.body, palette);

    // Load the wheel still, from whichever lib this car's set uses.
    SDL_Rect wheelSrcRect = {0, 0, 0, 0};
    SDL_Point wheelDrawSize = {0, 0};
    Bitmap* wheelBitmap = GetCarWheelFrame(model, car->GetWheelType(), 1,
                                           &wheelSrcRect, &wheelDrawSize);

    // Full-width strip from y=63 to y=146 (height=83)
    SDL_Rect dialogRect = {0, 63, 320, 83};

    // Wheels go behind the body, so the body's wheel arches cut over them -
    // the same order CarBitmap::Draw uses in the garage and diner. Lower
    // zOrder draws first.
    const int kWheelZ = 0;
    const int kBodyZ = 1;

    std::map<std::string, DialogBitmap> bitmaps;
    if (carBodyBitmap) {
        int carW = carBodyBitmap->GetSurface()->w;
        int carH = carBodyBitmap->GetSurface()->h;
        int carX = (320 - carW) / 2;
        int carY = 63 + 2;
        bitmaps["car_body"] = {carBodyBitmap, {carX, carY, carW, carH}, kBodyZ};

        // Add wheels relative to car body position. Frame 1 is the same still
        // the garage shows; where that frame lives, and how big it is, depends
        // on the car's set.
        if (wheelBitmap) {
            int rearWheelX = carX + offsets.offsetXRearTire;
            int frontWheelX = carX + offsets.offsetXFrontTire;
            int wheelY = carY + offsets.offsetYTires;
            bitmaps["rear_wheel"] = {wheelBitmap, {rearWheelX, wheelY, wheelDrawSize.x, wheelDrawSize.y}, kWheelZ, wheelSrcRect};
            bitmaps["front_wheel"] = {wheelBitmap, {frontWheelX, wheelY, wheelDrawSize.x, wheelDrawSize.y}, kWheelZ, wheelSrcRect};
        }
    }

    int textY = 63 + 83 - 2 - SFont_TextHeight(state.GetAssets().GetMainFont());
    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{0, textY, 320, 10}, std::string(carName), {0x70, 0x60, 0x10, 0xFF}, true}
    };

    DialogConfig cfg;
    cfg.bounds = dialogRect;
    cfg.backgroundColor = {0x30, 0x30, 0x30, 0xFF};
    cfg.borderColor = {0x30, 0x30, 0x30, 0xFF};
    cfg.texts = texts;
    cfg.buttons = {};
    cfg.contentAreas = std::vector<SDL_Rect>{dialogRect};
    cfg.bitmaps = bitmaps;
    cfg.font = state.GetAssets().GetMainFont();
    cfg.onDismiss = [this, carIndex]() {
        carViewDialog->Hide();
        SetActiveDialog(nullptr);
        ShowCarPurchaseDialog(carIndex);
    };

    carViewDialog = std::make_unique<Dialog>(cfg);
    carViewDialog->Show();
    SetActiveDialog(carViewDialog.get());
}
