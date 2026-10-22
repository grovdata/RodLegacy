#include "core/game_text.h"
#include "core/log.h"
#include "scenes/newspaper.h"
#include <sstream>
#include "core/state.h"
#include "car/car.h"
#include "car/car_builder.h"
#include "car/part.h"
#include "car/engine_part.h"
#include "gfx/animated_bitmap.h"
#include "car/carbitmap.h"
#include "parsers/carcolor.h"
#include "gfx/SFont.h"
#include "scenes/scene_layout.h"
#include "embedded_resources.h"
#include "ui/bubble.h"

const std::string autoParts = "------------ PARTS ------------";
const std::string usedCars = "---------- USED CARS ----------";

namespace {

PartCatalogEntry Engine(CarVendor_t v, CarEngine_t t) {
    PartCatalogEntry e{}; e.category = PartCategory::Engine; e.vendor = v; e.engineType = t; return e;
}
PartCatalogEntry Trans(CarVendor_t v, TransmissionType t) {
    PartCatalogEntry e{}; e.category = PartCategory::Transmission; e.vendor = v; e.transType = t; return e;
}
PartCatalogEntry Carb(CarburetorType t) {
    PartCatalogEntry e{}; e.category = PartCategory::Carburetor; e.vendor = CarVendorUnknown; e.carbType = t; return e;
}
PartCatalogEntry Manifold(CarVendor_t v, ManifoldType t) {
    PartCatalogEntry e{}; e.category = PartCategory::Manifold; e.vendor = v; e.manifoldType = t; return e;
}
PartCatalogEntry Tire(CarWheelType t) {
    PartCatalogEntry e{}; e.category = PartCategory::Tire; e.vendor = CarVendorUnknown; e.tireType = t; return e;
}

// Identity of each of the 40 AUTO PARTS slots, in executable order. Name and price come from partMetadata.
const PartCatalogEntry kPartCatalog[40] = {
    // Engines (6): "GM/Ford/Chrysler V-8 engine ..."
    Engine(CarVendorGM,       CarEngineGM_V8_282),
    Engine(CarVendorFord,     CarEngineFord_V8_292),
    Engine(CarVendorChrysler, CarEngineChrysler_V8_285),
    Engine(CarVendorGM,       CarEngineGM_V8_327),
    Engine(CarVendorFord,     CarEngineFord_V8_351),
    Engine(CarVendorChrysler, CarEngineChrysler_V8_340),

    // Transmissions (13)
    Trans(CarVendorGM,       TRANS_AUTO_GM),
    Trans(CarVendorFord,     TRANS_AUTO_FORD),
    Trans(CarVendorChrysler, TRANS_AUTO_CHRYSLER),
    Trans(CarVendorGM,       TRANS_3SPD_GM),
    Trans(CarVendorFord,     TRANS_3SPD_FORD),
    Trans(CarVendorChrysler, TRANS_3SPD_CHRYSLER),
    Trans(CarVendorGM,       TRANS_4SPD_GM),
    Trans(CarVendorFord,     TRANS_4SPD_FORD),
    Trans(CarVendorChrysler, TRANS_4SPD_CHRYSLER),
    Trans(CarVendorUnknown,  TRANS_4SPD_UNIVERSAL), // "Fits all"
    Trans(CarVendorGM,       TRANS_RACING_GM),
    Trans(CarVendorFord,     TRANS_RACING_FORD),
    Trans(CarVendorChrysler, TRANS_RACING_CHRYSLER),

    // Carburetors (3): "Fits all" -- not brand-locked in the ad text or in
    // CarburetorType/PartsCompatibilityManager.
    Carb(CARB_2BARREL),
    Carb(CARB_4BARREL),
    Carb(CARB_4BARREL_RACING),

    // Manifolds (15): 5 subtypes x 3 brands, in the ad's own order.
    Manifold(CarVendorGM,       MANIFOLD_2BARREL),
    Manifold(CarVendorFord,     MANIFOLD_2BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_2BARREL),
    Manifold(CarVendorGM,       MANIFOLD_4BARREL),
    Manifold(CarVendorFord,     MANIFOLD_4BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_4BARREL),
    Manifold(CarVendorGM,       MANIFOLD_DUAL_QUAD),      // "for 2-4brl carbs"
    Manifold(CarVendorFord,     MANIFOLD_DUAL_QUAD),
    Manifold(CarVendorChrysler, MANIFOLD_DUAL_QUAD),
    Manifold(CarVendorGM,       MANIFOLD_TRIPLE_2BARREL), // "for 3-2brl carbs"
    Manifold(CarVendorFord,     MANIFOLD_TRIPLE_2BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_TRIPLE_2BARREL),
    Manifold(CarVendorGM,       MANIFOLD_RACING),
    Manifold(CarVendorFord,     MANIFOLD_RACING),
    Manifold(CarVendorChrysler, MANIFOLD_RACING),

    // Tires (3): "Tires." / "Tires. Brand name." / "Racing slicks."
    Tire(STOCK),
    Tire(BRAND),
    Tire(RACING),
};

PartCatalogEntry Sr2Part(PartCategory c, CarVendor_t v, int cls) {
    PartCatalogEntry e{}; e.category = c; e.vendor = v; e.sr2Class = cls; return e;
}

// The 64 SR2 AUTO PARTS slots in executable order. SR2's small V8s (327/302/318) stand in as SR's 282/292/285 and its
// big V8s (427/429/426) as 327/351/340, the same two classes.
const PartCatalogEntry kSr2PartCatalog[64] = {
    Engine(CarVendorGM,       CarEngineGM_V6),
    Engine(CarVendorFord,     CarEngineFord_V6),
    Engine(CarVendorChrysler, CarEngineChrysler_V6),
    Engine(CarVendorGM,       CarEngineGM_V8_282),
    Engine(CarVendorFord,     CarEngineFord_V8_292),
    Engine(CarVendorChrysler, CarEngineChrysler_V8_285),
    Engine(CarVendorGM,       CarEngineGM_V8_327),
    Engine(CarVendorFord,     CarEngineFord_V8_351),
    Engine(CarVendorChrysler, CarEngineChrysler_V8_340),

    Trans(CarVendorGM,       TRANS_AUTO_GM),
    Trans(CarVendorFord,     TRANS_AUTO_FORD),
    Trans(CarVendorChrysler, TRANS_AUTO_CHRYSLER),
    Trans(CarVendorGM,       TRANS_3SPD_GM),
    Trans(CarVendorFord,     TRANS_3SPD_FORD),
    Trans(CarVendorChrysler, TRANS_3SPD_CHRYSLER),
    Trans(CarVendorGM,       TRANS_4SPD_GM),
    Trans(CarVendorFord,     TRANS_4SPD_FORD),
    Trans(CarVendorChrysler, TRANS_4SPD_CHRYSLER),
    Trans(CarVendorGM,       TRANS_RACING_GM),
    Trans(CarVendorFord,     TRANS_RACING_FORD),
    Trans(CarVendorChrysler, TRANS_RACING_CHRYSLER),

    Carb(CARB_2BARREL),
    Carb(CARB_4BARREL),
    Carb(CARB_4BARREL_RACING),

    Manifold(CarVendorGM,       MANIFOLD_2BARREL),
    Manifold(CarVendorFord,     MANIFOLD_2BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_2BARREL),
    Manifold(CarVendorGM,       MANIFOLD_4BARREL),
    Manifold(CarVendorFord,     MANIFOLD_4BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_4BARREL),
    // The ad texts are read in string order, where 2-4brl comes before 3-2brl; the records have them the other way.
    Manifold(CarVendorGM,       MANIFOLD_DUAL_QUAD),        // "for 2-4brl carbs"
    Manifold(CarVendorFord,     MANIFOLD_DUAL_QUAD),
    Manifold(CarVendorChrysler, MANIFOLD_DUAL_QUAD),
    Manifold(CarVendorGM,       MANIFOLD_TRIPLE_2BARREL),   // "for 3-2brl carbs"
    Manifold(CarVendorFord,     MANIFOLD_TRIPLE_2BARREL),
    Manifold(CarVendorChrysler, MANIFOLD_TRIPLE_2BARREL),
    Manifold(CarVendorGM,       MANIFOLD_RACING),
    Manifold(CarVendorFord,     MANIFOLD_RACING),
    Manifold(CarVendorChrysler, MANIFOLD_RACING),
    Manifold(CarVendorGM,       MANIFOLD_TUNNEL_RAM),   // tunnel ram
    Manifold(CarVendorFord, MANIFOLD_TUNNEL_RAM),
    Manifold(CarVendorChrysler, MANIFOLD_TUNNEL_RAM),
    Manifold(CarVendorGM,       MANIFOLD_DUAL_TUNNEL_RAM),   // dual tunnel ram
    Manifold(CarVendorFord, MANIFOLD_DUAL_TUNNEL_RAM),
    Manifold(CarVendorChrysler, MANIFOLD_DUAL_TUNNEL_RAM),
    Manifold(CarVendorGM,       MANIFOLD_FUEL_INJECTION),   // fuel injection
    Manifold(CarVendorFord, MANIFOLD_FUEL_INJECTION),
    Manifold(CarVendorChrysler, MANIFOLD_FUEL_INJECTION),
    Manifold(CarVendorGM,       MANIFOLD_SUPERCHARGER),   // supercharger
    Manifold(CarVendorChrysler, MANIFOLD_SUPERCHARGER),

    Tire(STOCK),
    Tire(BRAND),
    Tire(RACING),

    Sr2Part(PartCategory::ExhaustManifold, CarVendorGM,       SR_EXHAUST_STOCK),
    Sr2Part(PartCategory::ExhaustManifold, CarVendorFord,     SR_EXHAUST_STOCK),
    Sr2Part(PartCategory::ExhaustManifold, CarVendorChrysler, SR_EXHAUST_STOCK),
    Sr2Part(PartCategory::ExhaustManifold, CarVendorGM,       SR_EXHAUST_HEADERS),
    Sr2Part(PartCategory::ExhaustManifold, CarVendorFord,     SR_EXHAUST_HEADERS),
    Sr2Part(PartCategory::ExhaustManifold, CarVendorChrysler, SR_EXHAUST_HEADERS),

    Sr2Part(PartCategory::Muffler, CarVendorUnknown, SR_MUFFLER_STOCK),
    Sr2Part(PartCategory::Muffler, CarVendorUnknown, SR_MUFFLER_GLASSPACK),

    Sr2Part(PartCategory::Differential, CarVendorUnknown, SR_DIFF_2_78),
    Sr2Part(PartCategory::Differential, CarVendorUnknown, SR_DIFF_3_73),
    Sr2Part(PartCategory::Differential, CarVendorUnknown, SR_DIFF_4_11),
};

const PartCatalogEntry* PartCatalog() { return state.GetAssets().GetGame() == SrGame::SR2 ? kSr2PartCatalog : kPartCatalog; }
int PartCatalogCount() { return state.GetAssets().GetGame() == SrGame::SR2 ? 64 : 40; }

// The side-page art is a flat 240,240,240, so the ads column matches it.
constexpr Uint8 kPaperR = 240;
constexpr Uint8 kPaperG = 240;
constexpr Uint8 kPaperB = 240;

// The five front pages, in lib.p order.
constexpr AmigaLibAsset kHeadlineAssets[] = {
    NEWSPAPER_DODGERS_MALIBU,
    NEWSPAPER_YOUTH_KILLED,
    NEWSPAPER_OSCAR_NOMINATION,
    NEWSPAPER_GM_FORD_LAUGH_AT_JAPAN,
    NEWSPAPER_NO_SOLDIERS_IN_VIETNAM,
};
constexpr int kHeadlineCount = sizeof(kHeadlineAssets) / sizeof(kHeadlineAssets[0]);

// Street Rod 2 runs one front page a week, in this order (FUN_0025232e: day / 7, capped at 11).
constexpr int kSr2HeadlineAssets[] = {
    SR2_HEADLINE_SOVIETS_FIRST_STRIKE, SR2_HEADLINE_JUDY_GARLAND_DEAD, SR2_HEADLINE_NASA_MONKEY_IN_SPACE,
    SR2_HEADLINE_SPACE_MONKEY_DIES, SR2_HEADLINE_NIXON_VIETNAM, SR2_HEADLINE_DDT,
    SR2_HEADLINE_GREEN_BERET_SAIGON, SR2_HEADLINE_NIXON_VISITS, SR2_HEADLINE_KENNEDY_CAR_ACCIDENT,
    SR2_HEADLINE_WOODSTOCK, SR2_HEADLINE_PLANE_HIJACKED, SR2_HEADLINE_KHADAFY,
};
constexpr int kSr2HeadlineCount = sizeof(kSr2HeadlineAssets) / sizeof(kSr2HeadlineAssets[0]);

bool IsSr2() { return state.GetAssets().GetGame() == SrGame::SR2; }

int HeadlineCount() { return IsSr2() ? kSr2HeadlineCount : kHeadlineCount; }
int HeadlineAsset(int i) { return IsSr2() ? kSr2HeadlineAssets[i] : kHeadlineAssets[i]; }

Asset NewspaperBackground() {
    if (IsSr2()) return (Asset){LibAssetRegular, SrLibFileAmigaLibP, SR2_NEWSPAPER_FRONT, SrGame::SR2};
    return (Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_MENU};
}

Asset NewspaperExit() {
    if (IsSr2()) return (Asset){LibAssetRegular, SrLibFileAmigaLibP, SR2_NEWSPAPER_EXIT, SrGame::SR2};
    return (Asset){LibAssetRegular, SrLibFileAmigaLibP, NEWSPAPER_EXIT};
}

}

Newspaper::Newspaper() : Scene(NewspaperBackground()) {
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
        currentPage = 1 + totalPartsPages;
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
    GetZone("back_to_garage3").onClick = backToGarage;
    // Page 0 is the front page, then the AUTO PARTS pages, then the used-car pages. previous/next loop
    // within 1..lastPage; the front page is reached through its own zones.
    GetZone("previous").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        int lastPage = totalPartsPages + totalPages;
        if (currentPage > 1) currentPage--; else currentPage = lastPage;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };
    GetZone("next").onClick = [this] {
        if (sceneTransition != SCENE_SOLID) return;
        int lastPage = totalPartsPages + totalPages;
        if (currentPage < lastPage) currentPage++; else currentPage = 1;
        smoothScrollOffset = 0;
        LOG_INFO("Page: %d of %d", currentPage, totalPages);
        Update();
    };

    auto cantAffordBubble = std::make_unique<Bubble>(GameText::Msg(GameText::CANT_AFFORD_SHORT), BUBBLE_LEFT, true);
    cantAffordBubble->SetVisible(false);
    bubbles.push_back(std::move(cantAffordBubble));
}

Newspaper::~Newspaper() {
}

void Newspaper::ResetToFrontPage() {
    currentPage = 0;
    smoothScrollOffset = 0;
    if (IsSr2()) {
        headlineIndex = std::min(GameClock::DayNumber(state.GetElapsedTicks()) / 7, HeadlineCount() - 1);
    } else {
        // The original's choice of SR1 front page is unconfirmed; this walks them in order.
        headlineIndex = (headlineIndex + 1) % HeadlineCount();
    }
    SetHeadline(HeadlineAsset(headlineIndex));
    Update();
}

void Newspaper::Init(bool debugMode) {
    Scene::Init(debugMode);
    InitPageOne();
    RefreshClassifieds();

}

void Newspaper::InitPageOne() {
    previous = state.GetAssets().GetBitmapAsset(NewspaperExit());
    previous.SetPosition(0, 0);
    SDL_Rect previousRect = {0, 0, 64, 190};
    previous.Crop(previousRect);
    previous.SetDimensions(64, 190);
    previous.UpdateFromSurface();
    previous.SetVisible(false);

    next = state.GetAssets().GetBitmapAsset(NewspaperExit());
    next.SetPosition(GAME_WIDTH - 64, 0);
    SDL_Rect nextRect = {64, 0, 64, 190};
    next.Crop(nextRect);
    next.SetDimensions(64, 190);
    next.UpdateFromSurface();
    next.SetVisible(false);

    SetHeadline(HeadlineAsset(0));

    uint32_t dateColor = SDL_MapRGB(surface->format, 32, 32, 32);
    date.Init(state.GetAssets().GetMainFont(), 0, 120, dateColor);
    lastDateText = state.GetGameDateString();
    date.SetText("  " + lastDateText + "  ", true, GAME_WIDTH);
}

// The headline assets are finished 320x54 newsprint slabs, both columns included.
void Newspaper::SetHeadline(int asset) {
    headline = state.GetAssets().GetBitmapAsset(
        {LibAssetRegular, SrLibFileAmigaLibP, asset, IsSr2() ? SrGame::SR2 : SrGame::SR1});
    headline.SetPosition(0, 136);
    headline.SetVisible(true);
}

void Newspaper::RefreshClassifieds() {
    LoadClassifiedsData();
    InitPageAds();
    CreateAllAdsSurface();
    lastClassifiedsGeneration = state.GetClassifiedsGeneration();
    Update();
}

void Newspaper::LoadClassifiedsData() {
    // Rebuilt, not appended: it must stay the same length as state.classifiedCars.
    carMetadata.clear();
    carAssetIds.clear();
    partMetadata.clear();

    // Same order and skip condition as State, so it stays index-aligned with state.classifiedCars.
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

    // The AUTO PARTS ads are compiled into the Street_Rod executable.
    byte_vector& srExeData = state.GetAssets().GetSrExeData();
    int partCount = 0;
    DataParser dp;
    const int expectedParts = IsSr2() ? 64 : 40;
    PartMetadata_t* parts = dp.SrLibDataReadPartsFromBuffer(srExeData.data(), srExeData.size(), &partCount,
                                                            IsSr2() ? SrGame::SR2 : SrGame::SR1);
    if (parts && partCount == expectedParts) {
        partMetadata.assign(parts, parts + partCount);
    } else {
        LOG_INFO("WARNING: expected %d AUTO PARTS classifieds, got %d", expectedParts, partCount);
    }

    totalPartsPages = (partMetadata.size() + adsPerPage - 1) / adsPerPage;

    LOG_INFO("Loaded %zu parts classifieds, %d pages", partMetadata.size(), totalPartsPages);
}

void Newspaper::InitPageAds() {
    SDL_Rect textAreaRect = {64, 0, 192, 190};

    adsArea = Bitmap(textAreaRect.w, textAreaRect.h, 32);
    SDL_FillRect(adsArea.GetSurface(), NULL, SDL_MapRGBA(adsArea.GetSurface()->format, kPaperR, kPaperG, kPaperB, 255));
    adsArea.UpdateFromSurface();
    adsArea.SetPosition(textAreaRect.x, textAreaRect.y);
    adsArea.SetVisible(false);

    AddDivider(1);

    CreateClassifiedAds();
    CreateUsedCarAds();

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

    for (int i = 0; i < adsPerPage; i++) {
        Label adLabel;
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0);
        adLabel.Init(state.GetAssets().GetMainFont(), 68, 32, textColor);
        adLabel.SetVisible(false);

        classifiedAds.push_back(adLabel);
    }
}

void Newspaper::CreateUsedCarAds() {
    usedCarAds.clear();

    for (int i = 0; i < adsPerPage + 1; i++) {
        Label adLabel;
        uint32_t textColor = SDL_MapRGB(surface->format, 0, 0, 0);
        adLabel.Init(state.GetAssets().GetMainFont(), 68, 32, textColor);
        adLabel.SetVisible(false);

        usedCarAds.push_back(adLabel);
    }
}


void Newspaper::UpdatePageContent() {
    UpdateViewport();
}

void Newspaper::AddDivider(int /* y */) {
}

void Newspaper::DrawScene() {
    Scene::DrawScene();
    exit.Draw();

    headline.Draw();
    date.Draw();

    previous.Draw();
    adsArea.Draw();
    next.Draw();

    for (auto& ad : classifiedAds) {
        ad.Draw();
    }

    for (auto& ad : usedCarAds) {
        ad.Draw();
    }

    if (activeDialog && activeDialog->IsVisible()) {
        activeDialog->Draw();
    }

    for (auto& b : bubbles) {
        b->Draw();
    }
}

bool Newspaper::PushEvent(SDL_Event *event) {
    if (activeDialog && activeDialog->IsVisible()) {
        return activeDialog->HandleEvent(event);
    }

    if (event->type == SDL_MOUSEWHEEL && currentPage >= 1) {
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


std::vector<SDL_Rect> Newspaper::NavTargets() const {
    std::vector<SDL_Rect> out;
    for (const Zone& z : zones) {
        if (z.IsEnabled() && z.GetId() != "ads_area") out.push_back(z.GetRect());
    }
    if (currentPage == 0) return out;

    for (const AdPosition& ad : adPositions) {
        const int top = std::max(ad.startY, adsViewport.y);
        const int bottom = std::min(ad.endY, adsViewport.y + adsViewport.h);
        if (bottom > top) out.push_back({64, top - adsViewport.y, 192, bottom - top});
    }
    return out;
}

void Newspaper::Update() {
    // Unrelated to the per-frame Update(Uint64); refreshes page content.

    background->SetVisible(currentPage == 0);
    headline.SetVisible(currentPage == 0);
    date.SetVisible(currentPage == 0);
    previous.SetVisible(currentPage > 0);
    next.SetVisible(currentPage > 0);
    adsArea.SetVisible(currentPage > 0);

    SetZoneEnabled("auto_parts", currentPage == 0);
    SetZoneEnabled("used_cars", currentPage == 0);
    SetZoneEnabled("back_to_garage", currentPage == 0);
    SetZoneEnabled("back_to_garage3", currentPage != 0);
    SetZoneEnabled("previous", currentPage > 0);
    SetZoneEnabled("next", currentPage > 0);
    SetZoneEnabled("ads_area", currentPage > 0);

    UpdatePageContent();
}

void Newspaper::Update(Uint64 deltaTime) {
    Scene::Update(deltaTime);  // fade handling, otherwise lost by overriding this

    // A string compare every frame; the date only re-renders when the text changes.
    if (lastClassifiedsGeneration != state.GetClassifiedsGeneration()) {
        RefreshClassifieds();
    }

    std::string currentDateText = state.GetGameDateString();
    if (currentDateText != lastDateText) {
        lastDateText = currentDateText;
        date.SetText("  " + lastDateText + "  ", true, GAME_WIDTH);
    }
}

void Newspaper::HandleAdClick(int adIndex) {
    if (adIndex < 0 || adIndex >= (int)adPositions.size()) {
        return;
    }

    const AdPosition& adPos = adPositions[adIndex];
    if (adPos.isUsedCar) {
        if (adPos.adIndex < 0 || adPos.adIndex >= (int)carMetadata.size()) {
            return;
        }
        // carMetadata and classifiedCars are separate lists, only equal in length while they agree on the car sets.
        if (adPos.adIndex >= (int)state.GetClassifiedCars().size()) {
            LOG_WARN("Newspaper: ad %d has no classified car (%zu ads, %zu cars)",
                     adPos.adIndex, carMetadata.size(),
                     state.GetClassifiedCars().size());
            return;
        }
        if (!state.GetClassifiedCars()[adPos.adIndex]) {
            return;
        }
        selectedCarIndex = adPos.adIndex;
        ShowCarPurchaseDialog(adPos.adIndex);
    } else {
        if (adPos.adIndex < 0 || adPos.adIndex >= (int)partMetadata.size()) {
            return;
        }
        ShowPartPurchaseDialog(adPos.adIndex);
    }
}

void Newspaper::CreateAllAdsSurface() {
    LOG_INFO("CreateAllAdsSurface: Starting with %zu car metadata entries, %zu part entries",
             carMetadata.size(), partMetadata.size());

    // An upper bound to size the canvas; page counts come from the real rendered height below.
    int totalHeight = 0;

    totalHeight += 20; // PARTS header
    totalHeight += partMetadata.size() * 40;

    totalHeight += 20; // USED CARS header
    totalHeight += carMetadata.size() * 40;

    totalHeight += 100;

    allAdsSurfaceHeight = totalHeight;
    LOG_INFO("CreateAllAdsSurface: Calculated total height: %d", totalHeight);

    allAdsSurface = Bitmap(192, totalHeight, 32);
    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (!surface) {
        LOG_INFO("ERROR: Failed to create allAdsSurface - GetSurface() returned NULL");
        allAdsSurfaceHeight = 0;
        return;
    }
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, kPaperR, kPaperG, kPaperB, 255));

    adPositions.clear();

    SFont_Font* mainFont = state.GetAssets().GetMainFont();
    if (!mainFont) {
        LOG_INFO("ERROR: GetMainFont() returned NULL");
        return;
    }

    int currentY = 0;

    {
        Label headerLabel;
        uint32_t headerColor = SDL_MapRGB(surface->format, 32, 32, 32);
        headerLabel.Init(mainFont, 0, currentY, headerColor);
        headerLabel.SetText(autoParts, false, 184);
        headerLabel.UpdateFromSurface();

        int textWidth = SFont_TextWidth(state.GetAssets().GetMainFont(), autoParts.c_str());
        int centerX = (192 - textWidth) / 2;

        SDL_Rect headerRect = {centerX, currentY, 184, 20};
        SDL_BlitSurface(headerLabel.GetSurface(), NULL, allAdsSurface.GetSurface(), &headerRect);
        LOG_INFO("Rendered PARTS header at Y=%d", currentY);
        currentY += 20;
    }

    // Name at LEFT_COL, '+' feature line at RIGHT_COL on the same row, price on its own line.
    {
        const int LEFT_COL = 1;
        const int RIGHT_COL_MIN = 96;
        const int RIGHT_COL_GAP = 4;
        const int lineHeight = SFont_TextHeight(mainFont);

        SDL_SetSurfaceColorMod(mainFont->Surface, 0, 0, 0);

        currentY += 1;
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(surface, &topDividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
        currentY += 2;

        for (int i = 0; i < (int)partMetadata.size(); i++) {
            const ClassifiedEntry_t& entry = partMetadata[i].classified;
            const char* partName = entry.carName ? entry.carName : "";
            int adStartY = currentY;

            SFont_Write(surface, mainFont, LEFT_COL, currentY, partName);

            int nameWidth = SFont_TextWidth(mainFont, partName);
            int featureX = std::max(RIGHT_COL_MIN, LEFT_COL + nameWidth + RIGHT_COL_GAP);

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
            adPos.isUsedCar = false;
            adPositions.push_back(adPos);

            currentY += lineHeight;

            if (i < (int)partMetadata.size() - 1) {
                currentY += 4;
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(surface, &dividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
                currentY += 2;
            }
        }

        SDL_SetSurfaceColorMod(mainFont->Surface, 255, 255, 255);
    }

    partsSectionHeight = currentY;

    {
        Label headerLabel;
        uint32_t headerColor = SDL_MapRGB(surface->format, 32, 32, 32);
        headerLabel.Init(mainFont, 0, currentY, headerColor);
        headerLabel.SetText(usedCars, false, 184);
        headerLabel.UpdateFromSurface();

        int textWidth = SFont_TextWidth(mainFont, usedCars.c_str());
        int centerX = (192 - textWidth) / 2;

        SDL_Rect headerRect = {centerX, currentY, 184, 20};
        SDL_BlitSurface(headerLabel.GetSurface(), NULL, surface, &headerRect);
        LOG_INFO("Rendered USED CARS header at Y=%d", currentY);
        currentY += 20;
    }

    // As in the DOS layout: the name at LEFT_COL; '\t'-prefixed lines at RIGHT_COL on the same row, at most one
    // per entry; other lines advance y at LEFT_COL. The price is the last regular line.
    {
        const int LEFT_COL = 1;
        // Pushed right after long car names.
        const int RIGHT_COL_MIN = 96;
        const int RIGHT_COL_GAP = 4;
        const int lineHeight = SFont_TextHeight(mainFont);

        SDL_SetSurfaceColorMod(mainFont->Surface, 0, 0, 0);

        currentY += 1;
        SDL_Rect topDividerRect = {0, currentY, 192, 1};
        SDL_FillRect(surface, &topDividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
        currentY += 2;

        for (int i = 0; i < (int)carMetadata.size(); i++) {
            const ClassifiedEntry_t& entry = carMetadata[i].classified;
            const char* carName = entry.carName ? entry.carName : "";
            int adStartY = currentY;

            SFont_Write(surface, mainFont, LEFT_COL, currentY, carName);

            int carNameWidth = SFont_TextWidth(mainFont, carName);
            int featureX = std::max(RIGHT_COL_MIN, LEFT_COL + carNameWidth + RIGHT_COL_GAP);

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

            if (i < (int)carMetadata.size() - 1) {
                currentY += 4;
                SDL_Rect dividerRect = {0, currentY, 192, 1};
                SDL_FillRect(surface, &dividerRect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
                currentY += 2;
            }
        }

        SDL_SetSurfaceColorMod(mainFont->Surface, 255, 255, 255);
    }

    // From the real rendered height, or "next" reports pages with nothing to scroll to.
    const int usedCarsSectionHeight = currentY - partsSectionHeight;
    totalPartsPages = std::max(1, (partsSectionHeight + 189) / 190);
    totalPages = std::max(1, (usedCarsSectionHeight + 189) / 190);
    allAdsSurfaceHeight = currentY;
    LOG_INFO("Real content: parts %dpx (%d pages), cars %dpx (%d pages), total %dpx",
             partsSectionHeight, totalPartsPages, usedCarsSectionHeight, totalPages, allAdsSurfaceHeight);

    allAdsSurface.UpdateFromSurface();

    SDL_Surface* debugSurface = allAdsSurface.GetSurface();
    if (debugSurface) {
        LOG_INFO("Surface format: %dx%d, %d bpp, %s",
               debugSurface->w, debugSurface->h, debugSurface->format->BitsPerPixel,
               SDL_GetPixelFormatName(debugSurface->format->format));

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

    LOG_INFO("Created all ads surface: %dx%d", 192, allAdsSurfaceHeight);
    LOG_INFO("Total ads rendered: %zu", adPositions.size());
}

void Newspaper::UpdateViewport() {
    if (allAdsSurfaceHeight <= 0) {
        return;
    }

    SDL_Surface* allAdsSurfacePtr = allAdsSurface.GetSurface();
    SDL_Surface* adsAreaSurfacePtr = adsArea.GetSurface();
    if (!allAdsSurfacePtr || !adsAreaSurfacePtr) {
        return;
    }

    int viewportY = 0;

    if (currentPage >= 1 && currentPage <= totalPartsPages) {
        int pageScrollOffset = (currentPage - 1) * 190; // 190px per "page" of scrolling
        viewportY = pageScrollOffset + smoothScrollOffset;

        int maxViewportY = std::max(0, partsSectionHeight - 190);
        if (viewportY > maxViewportY) {
            viewportY = maxViewportY;
            smoothScrollOffset = viewportY - pageScrollOffset;
        }
        if (viewportY < 0) {
            viewportY = 0;
            smoothScrollOffset = 0;
        }
    } else if (currentPage > totalPartsPages) {
        int pageScrollOffset = (currentPage - totalPartsPages - 1) * 190;

        viewportY = partsSectionHeight + pageScrollOffset;

        viewportY += smoothScrollOffset;


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

    adsViewport = {0, viewportY, 192, 190};

    SDL_FillRect(adsAreaSurfacePtr, NULL, SDL_MapRGBA(adsAreaSurfacePtr->format, kPaperR, kPaperG, kPaperB, 255));

        if (viewportY < allAdsSurfaceHeight) {
            SDL_Rect srcRect = adsViewport;
            SDL_Rect destRect = {0, 0, 192, 190};

            if (allAdsSurfacePtr->format->format != adsAreaSurfacePtr->format->format) {
                SDL_Surface* convertedSurface = SDL_ConvertSurface(allAdsSurfacePtr, adsAreaSurfacePtr->format, 0);
                if (convertedSurface) {
                    SDL_BlitSurface(convertedSurface, &srcRect, adsAreaSurfacePtr, &destRect);
                    SDL_FreeSurface(convertedSurface);
                }
            } else {
                SDL_BlitSurface(allAdsSurfacePtr, &srcRect, adsAreaSurfacePtr, &destRect);
            }
        }

    adsArea.UpdateFromSurface();
}

void Newspaper::SaveAdsSurfaceToFile() {
    LOG_INFO("Saving all ads surface to PNG...");

    SDL_Surface* surface = allAdsSurface.GetSurface();
    if (surface) {
        LOG_INFO("Surface format: %dx%d, %d bpp, %s",
               surface->w, surface->h, surface->format->BitsPerPixel,
               SDL_GetPixelFormatName(surface->format->format));
    }

    allAdsSurface.SaveBitmap(999);
    LOG_INFO("Saved all ads surface to sprite_999.png");
}

int Newspaper::FindAdAtPosition(int mouseY) {
    int surfaceY = mouseY + adsViewport.y;

    // Returns the position within adPositions, not adPos.adIndex, which collides between parts and cars.
    for (int i = 0; i < (int)adPositions.size(); i++) {
        if (surfaceY >= adPositions[i].startY && surfaceY < adPositions[i].endY) {
            return i;
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
    buttons[2].cancelChoice = true;

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
                    bubbles[0]->SetText("  " + GameText::Msg(GameText::CANT_AFFORD_SHORT, short_));
                    bubbles[0]->SetVisible(true);
                } else if (!state.GetClassifiedCars()[carIndex]) {
                    LOG_INFO("Car %d already sold", carIndex);
                } else {
                    state.SetBankroll(state.GetBankroll() - price);
                    state.AdvanceTicks(600);
                    CarPtr boughtCar = state.GetClassifiedCars()[carIndex];
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

void Newspaper::ShowPartPurchaseDialog(int partIndex) {
    if (partIndex < 0 || partIndex >= (int)partMetadata.size()) {
        return;
    }

    const ClassifiedEntry_t& classified = partMetadata[partIndex].classified;
    const char* partName = classified.carName ? classified.carName : "Unknown Part";
    int price = classified.price;

    SDL_Rect dialogRect = {34, 97, 252, 46};
    SDL_Rect yeahRect = {88, 123, 66, 16};
    SDL_Rect forgetItRect = {166, 123, 66, 16};

    std::vector<ButtonConfig> buttons = {
        {yeahRect, "Yeah", nullptr, true, 0},
        {forgetItRect, "Forget it", nullptr, false, 1}
    };
    buttons[1].cancelChoice = true;

    std::string priceText = "You want to buy it for $" + std::to_string(price) + " ?";

    std::vector<DialogTextConfig> texts = {
        DialogTextConfig{SDL_Rect{38, 101, 248, 10}, std::string(partName), {0x00, 0x00, 0x00, 0xFF}, true},
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
    dialogConfig.onButtonPressed = [this, partIndex, price](int buttonIndex) {
        switch (buttonIndex) {
            case 0: { // Yeah - buy the part
                if (state.GetBankroll() < price) {
                    LOG_INFO("Not enough money to buy part %d (need $%d, have $%d)",
                            partIndex, price, state.GetBankroll());
                    int short_ = price - state.GetBankroll();
                    bubbles[0]->SetText("  " + GameText::Msg(GameText::CANT_AFFORD_SHORT, short_));
                    bubbles[0]->SetVisible(true);
                } else {
                    // Costs no game time in SR1, and the parts catalog is standing, so stay on the paper.
                    state.SetBankroll(state.GetBankroll() - price);
                    state.AdvanceTicks(300);

                    const char* rawName = partMetadata[partIndex].classified.carName;
                    std::string adName = rawName ? rawName : "Unknown Part";

                    const PartCatalogEntry& entry = PartCatalog()[partIndex];
                    switch (entry.category) {
                        case PartCategory::Engine: {
                            EnginePart part(entry.engineType, 0, price, entry.vendor,
                                            (AmigaLibAsset)entry.engineType);
                            part.name = adName;
                            state.AddSpareEngine(part);
                            break;
                        }
                        case PartCategory::Transmission: {
                            Part part = CarBuilder::CreateTransmissionPart(entry.transType, entry.vendor);
                            part.cost = price;
                            part.name = adName;
                            state.AddSpareTransmission(part);
                            break;
                        }
                        case PartCategory::Carburetor: {
                            Part part = CarBuilder::CreateCarburetorPart(entry.carbType, entry.vendor);
                            part.cost = price;
                            part.name = adName;
                            state.AddSpareCarburetor(part);
                            break;
                        }
                        case PartCategory::Manifold: {
                            Part part = CarBuilder::CreateManifoldPart(entry.manifoldType, entry.vendor);
                            part.cost = price;
                            part.name = adName;
                            state.AddSpareManifold(part);
                            break;
                        }
                        case PartCategory::Tire: {
                            Part part(0, price, entry.vendor,
                                      TireAssetForType(entry.tireType), entry.tireType);
                            part.name = adName;
                            state.AddSpareTire(part);
                            break;
                        }
                        case PartCategory::ExhaustManifold:
                        case PartCategory::Muffler:
                        case PartCategory::Differential: {
                            Part part = MakeSr2Part(entry.category, entry.sr2Class, entry.vendor);
                            part.cost = price;
                            part.name = adName;
                            state.GetSpareSr2(entry.category).push_back(part);
                            break;
                        }
                    }

                    LOG_INFO("Bought part %d (%s) for $%d, bankroll now $%d",
                            partIndex, adName.c_str(), price, state.GetBankroll());
                }
                partPurchaseDialog->Hide();
                SetActiveDialog(nullptr);
                break;
            }
            case 1: // Forget it
                partPurchaseDialog->Hide();
                SetActiveDialog(nullptr);
                break;
        }
    };

    partPurchaseDialog = std::make_unique<Dialog>(dialogConfig);
    partPurchaseDialog->Show();
    SetActiveDialog(partPurchaseDialog.get());
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

    const CarPtr& car = state.GetClassifiedCars()[carIndex];
    if (!car) {
        return; // Already sold
    }

    CarModel model = car->GetModel();
    BitmapOffset_t offsets = model.offsets;
    std::vector<uint16_t> palette = GetStandardCarColor(car->GetColor());

    // carAssetIds holds the pool key, which for an SE car is not a lib.p asset.
    Bitmap* carBodyBitmap = LoadCarModelSprite(model, model.sprites.body, palette);

    SDL_Rect wheelSrcRect = {0, 0, 0, 0};
    SDL_Point wheelDrawSize = {0, 0};
    Bitmap* wheelBitmap = GetCarWheelFrame(model, car->GetWheelType(), 1,
                                           &wheelSrcRect, &wheelDrawSize);

    SDL_Rect dialogRect = {0, 63, 320, 83};

    // Wheels go behind the body so its arches cut over them. Lower zOrder draws first.
    const int kWheelZ = 0;
    const int kBodyZ = 1;

    std::map<std::string, DialogBitmap> bitmaps;
    if (carBodyBitmap) {
        int carW = carBodyBitmap->GetSurface()->w;
        int carH = carBodyBitmap->GetSurface()->h;
        int carX = (320 - carW) / 2;
        int carY = 63 + 2;
        bitmaps["car_body"] = {carBodyBitmap, {carX, carY, carW, carH}, kBodyZ};

        // Frame 1 is the garage's still.
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

std::string CatalogPartName(PartCategory category, CarVendor_t vendor, int typeValue) {
    static SrGame loadedFor = SrGame::SR1;
    static std::vector<std::string> names;
    const SrGame game = state.GetAssets().GetGame();
    if (names.empty() || loadedFor != game) {
        names.clear();
        loadedFor = game;
        byte_vector& exe = state.GetAssets().GetSrExeData();
        int count = 0;
        DataParser dp;
        PartMetadata_t* parts = dp.SrLibDataReadPartsFromBuffer(exe.data(), exe.size(), &count, game);
        for (int i = 0; parts && i < count; i++) {
            names.push_back(parts[i].classified.carName ? parts[i].classified.carName : "");
        }
    }
    for (int i = 0; i < PartCatalogCount() && i < static_cast<int>(names.size()); i++) {
        const PartCatalogEntry& e = PartCatalog()[i];
        if (e.category != category) continue;
        int entryType;
        switch (category) {
            case PartCategory::Engine:          entryType = static_cast<int>(e.engineType); break;
            case PartCategory::Transmission:    entryType = static_cast<int>(e.transType); break;
            case PartCategory::Carburetor:      entryType = static_cast<int>(e.carbType); break;
            case PartCategory::Manifold:        entryType = static_cast<int>(e.manifoldType); break;
            case PartCategory::Tire:            entryType = static_cast<int>(e.tireType); break;
            case PartCategory::ExhaustManifold:
            case PartCategory::Muffler:
            case PartCategory::Differential:    entryType = e.sr2Class; break;
        }
        if (entryType != typeValue) continue;
        if (e.vendor != CarVendorUnknown && e.vendor != vendor) continue;
        return names[i];
    }
    return "";
}

std::string Newspaper::FindCatalogName(PartCategory category, CarVendor_t vendor, int typeValue) const {
    for (int i = 0; i < PartCatalogCount() && i < (int)partMetadata.size(); i++) {
        const PartCatalogEntry& e = PartCatalog()[i];
        if (e.category != category) continue;
        int entryType;
        switch (category) {
            case PartCategory::Engine:       entryType = (int)e.engineType; break;
            case PartCategory::Transmission: entryType = (int)e.transType; break;
            case PartCategory::Carburetor:   entryType = (int)e.carbType; break;
            case PartCategory::Manifold:     entryType = (int)e.manifoldType; break;
            case PartCategory::Tire:         entryType = (int)e.tireType; break;
            default: continue;
        }
        if (entryType != typeValue) continue;
        if (e.vendor != CarVendorUnknown && e.vendor != vendor) continue;
        const char* name = partMetadata[i].classified.carName;
        return name ? name : "";
    }
    return "";
}

std::string Newspaper::FindEngineCatalogName(CarEngine_t type, CarVendor_t vendor) const {
    return FindCatalogName(PartCategory::Engine, vendor, (int)type);
}
std::string Newspaper::FindTransmissionCatalogName(TransmissionType type, CarVendor_t vendor) const {
    return FindCatalogName(PartCategory::Transmission, vendor, (int)type);
}
std::string Newspaper::FindCarburetorCatalogName(CarburetorType type) const {
    return FindCatalogName(PartCategory::Carburetor, CarVendorUnknown, (int)type);
}
std::string Newspaper::FindManifoldCatalogName(ManifoldType type, CarVendor_t vendor) const {
    return FindCatalogName(PartCategory::Manifold, vendor, (int)type);
}
std::string Newspaper::FindTireCatalogName(CarWheelType type) const {
    return FindCatalogName(PartCategory::Tire, CarVendorUnknown, (int)type);
}
