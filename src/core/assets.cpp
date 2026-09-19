#include "core/log.h"
#include "core/assets.h"

#include <exepack/unpack.hpp>

#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "parsers/adf.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "core/embedded_loader.h"
#include "ui/main_font.h"
#include "core/zip_utils.h"
#include "race/race_sim.h"
#include <functional>
#include <fstream>

// Hash function for palette vectors
static size_t hashPalette(const std::vector<uint16_t>& palette) {
    size_t hash = 0;
    for (uint16_t val : palette) {
        hash ^= std::hash<uint16_t>{}(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

// Generated embedded resources
extern "C" {
    #include "embedded_resources.h"
}

AssetManager::AssetManager() : carDecals(nullptr), audioManager(nullptr) {
    // Sized up front so LoadSettings, which runs before the packs are read,
    // has somewhere to record which optional sets are switched on.
    carSets.resize(GetCarSetDefs().size());
}

AssetManager::~AssetManager() {
    if (mainFont) {
        SDL_FreeSurface(mainFont->Surface);
        free(mainFont);
    }
    if (debugFont) {
        SDL_FreeSurface(debugFont->Surface);
        free(debugFont);
    }
    if (newspaperFont) {
        SDL_FreeSurface(newspaperFont->Surface);
        free(newspaperFont);
    }
    if (audioManager) {
        delete audioManager;
    }
}

void AssetManager::InitFonts() {
    LOG_INFO("Loading fonts from embedded resources");

    // Load newspaper font from embedded data
    SDL_Surface* newspaperFontSurface = LoadSurfaceFromMemory(
        embedded_bee_png,
        embedded_bee_png_len
    );
    if (!newspaperFontSurface) {
        throw AssetLoadError("Failed to load embedded newspaper font");
    }
    newspaperFont = SFont_InitFont(newspaperFontSurface);
    if (!newspaperFont) {
        SDL_FreeSurface(newspaperFontSurface);
        throw AssetLoadError("Failed to initialize newspaper font");
    }

    // Load debug font from embedded data
    SDL_Surface* debugFontSurface = LoadSurfaceFromMemory(
        embedded_lemon_png,
        embedded_lemon_png_len
    );
    if (!debugFontSurface) {
        throw AssetLoadError("Failed to load embedded debug font");
    }
    debugFont = SFont_InitFont(debugFontSurface);
    if (!debugFont) {
        SDL_FreeSurface(debugFontSurface);
        throw AssetLoadError("Failed to initialize debug font");
    }

    LOG_INFO("Newspaper and debug fonts loaded from embedded resources");
}

void AssetManager::LoadAmigaFontFromExe() {
    SFont_Font* amigaFont = LoadMainFontFromExe(srExeData_vec);
    if (!amigaFont) {
        return;
    }

    if (mainFont) {
        SDL_FreeSurface(mainFont->Surface);
        free(mainFont);
    }
    mainFont = amigaFont;
    LOG_INFO("Main font loaded from Amiga Street_Rod executable");
}

void AssetManager::InitMousePointers() {
    uint16_t* srExeData_uint16 = reinterpret_cast<uint16_t*>(srExeData_vec.data());

    for (int i = 0; i < SrLibAmigaSpriteAssetCount; i++) {
        SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[i];
        std::vector<SrLibColor> buffer(info.width * info.height);
        SrLibAmigaSpriteAsset pointer = (SrLibAmigaSpriteAsset)i;
        SrLibAmigaSpriteReadMousePointer(srExeData_uint16, pointer, buffer.data());
        std::unique_ptr<Bitmap> mousePointer = std::make_unique<Bitmap>();
        mousePointer->LoadBitmapFromChunky(buffer, info.width, info.height);
        mousePointer->SetDimensions(info.width, info.height);
        mousePointers[pointer] = std::move(mousePointer);
    }
}

// Helper: extract a named file from an ADF, which may be on disk or in a memory buffer.
// Returns a byte vector; throws AssetLoadError on failure.
static byte_vector ExtractFromAdf(const fs::path* adfPath,
                                   const byte_vector* adfBuf,
                                   const char* fileName) {
    AdfFileData* data = nullptr;
    if (adfPath) {
        data = extract_adf_file_to_memory(adfPath->string().c_str(), fileName);
    } else if (adfBuf) {
        data = extract_adf_file_from_buffer(adfBuf->data(), adfBuf->size(), fileName);
    }
    if (!data) {
        throw AssetLoadError(std::string("Failed to extract ") + fileName + " from ADF");
    }
    byte_vector result(static_cast<uint8_t*>(data->data),
                       static_cast<uint8_t*>(data->data) + data->size);
    free_adf_file_data(data);
    return result;
}

void AssetManager::InitGameFiles() {
    const char* disk1Name = SrLibArchiveNames[SrLibArchiveSR1AmigaDisk1]; // "SR1AMIGA_DISK1.adf"
    const char* disk2Name = SrLibArchiveNames[SrLibArchiveSR1AmigaDisk2]; // "SR1AMIGA_DISK2.adf"

    fs::path disk1Direct = gameBasePath / disk1Name;
    fs::path disk2Direct = gameBasePath / disk2Name;
    fs::path zipPath     = gameBasePath / "sr1amiga.zip";

    bool useDirectAdfs = fs::exists(disk1Direct) && fs::exists(disk2Direct);
    bool useZip        = !useDirectAdfs && fs::exists(zipPath);

    if (!useDirectAdfs && !useZip) {
        throw AssetLoadError("No ADF files or sr1amiga.zip found in game directory");
    }

    // For direct ADF files, verify checksums (existing behaviour).
    if (useDirectAdfs) {
        SrLibStatus s1 = SrLibVerifyChecksum(disk1Direct.string().c_str(),
                                              SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk1]);
        if (s1 != SrLibStatusSuccess) {
            throw AssetLoadError("SR1AMIGA_DISK1.adf is corrupted (checksum mismatch)");
        }
        SrLibStatus s2 = SrLibVerifyChecksum(disk2Direct.string().c_str(),
                                              SrLibArchiveChecksums[SrLibArchiveSR1AmigaDisk2]);
        if (s2 != SrLibStatusSuccess) {
            throw AssetLoadError("SR1AMIGA_DISK2.adf is corrupted (checksum mismatch)");
        }
    }

    // Load ADF bytes: either read file path or extract from zip into memory.
    const fs::path* path1 = nullptr;
    const fs::path* path2 = nullptr;
    byte_vector disk1Buf, disk2Buf;

    if (useDirectAdfs) {
        LOG_INFO("Loading game files from ADF files in %s", gameBasePath.string().c_str());
        // Read full disk1 ADF into memory for audio use
        std::ifstream f(disk1Direct, std::ios::binary | std::ios::ate);
        if (f) {
            auto sz = f.tellg();
            f.seekg(0);
            disk1Adf_vec.resize(static_cast<size_t>(sz));
            f.read(reinterpret_cast<char*>(disk1Adf_vec.data()), sz);
        }
        // Read full disk2 ADF into memory too -- it ships sample .sav files
        // in its root directory, listed later for the Load Game menu.
        std::ifstream f2(disk2Direct, std::ios::binary | std::ios::ate);
        if (f2) {
            auto sz = f2.tellg();
            f2.seekg(0);
            disk2Adf_vec.resize(static_cast<size_t>(sz));
            f2.read(reinterpret_cast<char*>(disk2Adf_vec.data()), sz);
        }
        path1 = &disk1Direct;
        path2 = &disk2Direct;
    } else {
        LOG_INFO("Loading game files from sr1amiga.zip in %s", gameBasePath.string().c_str());
        disk1Buf = ZipExtractToMemory(zipPath, disk1Name);
        disk2Buf = ZipExtractToMemory(zipPath, disk2Name);
        if (disk1Buf.empty() || disk2Buf.empty()) {
            throw AssetLoadError("Failed to extract ADF files from sr1amiga.zip");
        }
        disk1Adf_vec = disk1Buf;  // keep a copy for audio
        disk2Adf_vec = disk2Buf;  // keep a copy for sample-save listing
    }

    srExeData_vec    = ExtractFromAdf(path1, useDirectAdfs ? nullptr : &disk1Buf, "Street_Rod");
    libStartData_vec = ExtractFromAdf(path1, useDirectAdfs ? nullptr : &disk1Buf, "libstart.p");
    libPData_vec     = ExtractFromAdf(path2, useDirectAdfs ? nullptr : &disk2Buf, "lib.p");
    srData_vec       = ExtractFromAdf(path2, useDirectAdfs ? nullptr : &disk2Buf, "SR_Data");

    LOG_INFO("Found all required Amiga game files.");
}

void AssetManager::InitStockPalettes() {
    stockPalettes[StockPaletteAmigaDiner] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDiner), true);
    stockPalettes[StockPaletteAmigaGasStation] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaGasStation), true);
    stockPalettes[StockPaletteAmigaDriving] = bp.PreparePalette(bp.GetStockPalette(StockPaletteAmigaDriving), true);
}

void AssetManager::InitAudio(bool muteAudio) {
    if (muteAudio) {
        LOG_INFO("Audio system disabled (muted)");
        return;
    }

    LOG_INFO("Initializing audio system");

    audioManager = new AudioManager();

    if (!audioManager->Initialize(disk1Adf_vec.data(), disk1Adf_vec.size())) {
        LOG_INFO("Warning: Audio initialization failed, continuing without audio");
        delete audioManager;
        audioManager = nullptr;
    } else {
        LOG_INFO("Audio system initialized successfully");

        if (!disk1Adf_vec.empty()) {
            audioManager->InitAmigaMusic(disk1Adf_vec.data(), disk1Adf_vec.size());
        }

        // Load SR.EXE: try on-disk first, then inside sr1pc.zip.
        byte_vector srExePacked;
        fs::path srExePath    = gameBasePath / "SR.EXE";
        fs::path sr1pcZipPath = gameBasePath / "sr1pc.zip";

        if (fs::exists(srExePath)) {
            std::ifstream f(srExePath, std::ios::binary | std::ios::ate);
            if (f) {
                auto sz = f.tellg();
                f.seekg(0);
                srExePacked.resize(static_cast<size_t>(sz));
                if (!f.read(reinterpret_cast<char*>(srExePacked.data()), sz))
                    srExePacked.clear();
            }
        } else if (fs::exists(sr1pcZipPath)) {
            srExePacked = ZipExtractToMemory(sr1pcZipPath, "SR1/SR.EXE");
            if (srExePacked.empty())
                LOG_INFO("sr1pc.zip found but SR1/SR.EXE not inside, no PC speaker music");
        }

        if (!srExePacked.empty()) {
            exepack::UnpackExeResult unpacked =
                exepack::unpack_exepack_to_exe_bytes(srExePacked.data(), srExePacked.size());
            if (!unpacked.ok()) {
                LOG_INFO("EXEPACK unpack of SR.EXE failed: %s", unpacked.error.c_str());
            } else {
                dosSrExeUnpacked_vec = std::move(unpacked.exe_bytes);
                audioManager->InitPCSpeakerFromExeBytes(dosSrExeUnpacked_vec.data(),
                                                        dosSrExeUnpacked_vec.size());
                LOG_INFO("PC speaker music loaded from %s",
                         fs::exists(srExePath) ? "SR.EXE" : "sr1pc.zip");
            }
        } else if (!fs::exists(srExePath) && !fs::exists(sr1pcZipPath)) {
            LOG_INFO("No SR.EXE or sr1pc.zip found, no PC speaker music");
        }
    }
}

void AssetManager::Init(const fs::path& validatedPath, bool muteAudio) {
    try {
        gameBasePath = validatedPath;
        dataPath = gameBasePath;
        InitFonts();
        InitGameFiles();
        LoadAmigaFontFromExe();
        InitStockPalettes();
        InitAudio(muteAudio);
        LoadSettings();
    } catch (const AssetLoadError& e) {
        LOG_ERROR("Failed to initialize asset manager: %s", e.what());
        throw;
    }

}

SFont_Font* AssetManager::GetMainFont() {
    return mainFont;
}

SFont_Font* AssetManager::GetDebugFont() {
    return debugFont;
}

SFont_Font* AssetManager::GetNewspaperFont() {
    return newspaperFont;
}

Palette AssetManager::GetStockPalette(StockPalette palette) {
    auto it = stockPalettes.find(palette);
    if (it == stockPalettes.end()) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GetStockPalette: palette %d not found", (int)palette);
        return stockPalettes.at(StockPaletteAmigaDiner);
    }
    return it->second;
}

void AssetManager::ReadAssets() {
    try {
        imageEntries = bp.ReadEntries(libPData_vec);

        for (int i = 0; i < imageEntries.first; i++) {
            UpdateAsset(libPData_vec, i);
        }

        if (carDecals == nullptr) {
            auto *decals = (AnimatedBitmap*)GetBitmapAsset({LibAssetSpritesheet, SrLibFileAmigaLibP, DECALS});
            if (!decals) {
                throw AssetLoadError("Failed to load car decals");
            }
            SDL_Rect cropDecal = {4, 9, 222, 51};
            decals->Crop(cropDecal);
            carDecals = decals;
        }

        InitMousePointers();
        InitDosAssets();
        InitCarSets();

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to read assets: %s", e.what());
        throw;
    }
}

void AssetManager::UpdateAsset(byte_vector& libfile, int index) {
    Palette empty;
    UpdateAsset(libfile, index, empty);
}


/// @brief Reads the actual bitmap data for a bitmap
/// @param libfile
/// @param index
/// @param carPalette
void AssetManager::UpdateAsset(byte_vector& libfile, int index, Palette& carPalette) {
    // Guard the subscript: index is a lib.p asset id, and anything sourced
    // from a car pool key can be outside it (Street Rod SE cars carry a
    // shifted index). Log rather than trap - the caller wanted lib.p art and
    // there is none, so there is nothing to draw.
    if (index < 0 || index >= (int)imageEntries.second.size()) {
        LOG_ERROR("UpdateAsset: index %d outside lib.p (%zu entries)", index, imageEntries.second.size());
        return;
    }
    BitmapEntry_t b = imageEntries.second[index];
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);
    Palette pal = bp.PreparePalette(palData, true);
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette, index);

    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);

    bool isMask = a.type == LibAssetMask;
    const bool loadOpaque = bp.LoadsOpaque(SrLibFileAmigaLibP, index);
    // Bumper overlays use normal transparency (black = transparent, colors = opaque)
    // They don't need special handling

    if (a.type == LibAssetSpritesheet) {
        std::pair<int, int> info = bp.GetSpritesheetInfo(SrLibFileAmigaLibP, index);
        std::unique_ptr<AnimatedBitmap> spriteSheet = std::make_unique<AnimatedBitmap>();
        spriteSheet->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, loadOpaque);
        spriteSheet->SetDimensions(b.width, b.height);
        if (info.first > 0 && info.second > 0) {
            spriteSheet->SetGrid(info.first, info.second);
        }
        imageAssets.push_back(std::make_pair(a, std::move(spriteSheet)));
    } else {
        std::unique_ptr<Bitmap> bitmap = std::make_unique<Bitmap>();
        bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, loadOpaque);
        bitmap->SetDimensions(b.width, b.height);
        imageAssets.push_back(std::make_pair(a, std::move(bitmap)));
    }
}


void AssetManager::UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette) {
    std::vector<uint16_t> carPalette = GetStandardCarColor(palette);
    Palette newPalette = bp.PrepareCarPalette(carPalette);
    UpdateAsset(libfile, index, newPalette); // Update the asset with the new color
}

Bitmap* AssetManager::GetBitmapAsset(const Asset& asset) {
    for (auto& i : imageAssets) {
        if (i.first == asset) {
            return i.second.get();
        }
    }

    LOG_ERROR("Warning: Asset not found (wrong type of asset?): %d", asset.id);
    return nullptr;
}

AnimatedBitmap* AssetManager::GetCarDecals() {
    return this->carDecals;
}

Bitmap* AssetManager::GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette) {
    // Create cache key: asset ID + palette hash
    size_t paletteHash = hashPalette(carPalette);
    AssetPaletteKey cacheKey{asset.id, paletteHash};

    // Check if this asset+palette combination is already cached
    auto it = paletteVariantCache.find(cacheKey);
    if (it != paletteVariantCache.end()) {
        return it->second.get();  // Return cached version
    }

    // Not in cache, create new variant and cache it
    CreateAndCachePaletteVariant(libfile, asset.id, carPalette, cacheKey);
    return paletteVariantCache[cacheKey].get();
}

void AssetManager::CreateAndCachePaletteVariant(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey) {
    if (index < 0 || index >= (int)imageEntries.second.size()) {
        LOG_ERROR("CreateAndCachePaletteVariant: index %d outside lib.p (%zu entries)",
                  index, imageEntries.second.size());
        return;
    }
    BitmapEntry_t b = imageEntries.second[index];
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, imageEntries.first);
    std::vector<uint16_t> palData = bp.GetPalette(SrLibFileAmigaLibP, index);

    // Convert car palette to RGB format
    Palette processedCarPalette = bp.PrepareCarPalette(carPalette);
    Palette basePal = bp.PreparePalette(palData, true);

    // Use provided car palette for color ranges
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette, index);

    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index);

    bool isMask = a.type == LibAssetMask;
    const bool loadOpaque = bp.LoadsOpaque(SrLibFileAmigaLibP, index);

    std::unique_ptr<Bitmap> bitmap = std::make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, loadOpaque);
    bitmap->SetDimensions(b.width, b.height);

    // Store in palette variant cache (not in imageAssets to prevent unbounded growth)
    paletteVariantCache[cacheKey] = std::move(bitmap);
}

byte_vector AssetManager::LoadDosDataFile(const fs::path& loosePath, const fs::path& zipPath,
        const std::string& zipEntry, const char* what) {
    byte_vector data;

    if (fs::exists(loosePath)) {
        std::ifstream file(loosePath, std::ios::binary | std::ios::ate);
        if (!file) {
            LOG_INFO("Failed to open %s at %s", what, loosePath.string().c_str());
            return {};
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        data.resize(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            LOG_INFO("Failed to read %s at %s", what, loosePath.string().c_str());
            return {};
        }
        return data;
    }

    if (fs::exists(zipPath)) {
        data = ZipExtractToMemory(zipPath, zipEntry);
        if (data.empty()) {
            LOG_INFO("%s found but %s not inside, %s unavailable",
                     zipPath.filename().string().c_str(), zipEntry.c_str(), what);
        }
        return data;
    }

    LOG_INFO("No %s or %s found, %s unavailable",
             loosePath.filename().string().c_str(), zipPath.filename().string().c_str(), what);
    return {};
}

std::unique_ptr<Bitmap> AssetManager::DecodeDosSprite(byte_vector& libData,
        const std::pair<int, std::vector<DOSBitmapEntry_t>>& entries, int index,
        const std::vector<uint16_t>* carPalette) {
    if (index < 0 || index >= entries.first) {
        return nullptr;
    }

    DOSBitmapEntry_t entry = entries.second[index];
    std::vector<uint8_t> planarData = bp.ReadImageDOS(libData, entry, index, entries.first);
    Palette palette = carPalette ? bp.GetDOSCarPalette(entry.type, *carPalette)
                                 : bp.GetDOSPalette(entry.type);
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(planarData, entry, palette);

    auto bitmap = std::make_unique<Bitmap>();
    // Load as opaque so black pixels are preserved (not made transparent)
    bitmap->LoadBitmapFromChunky(chunkyData, entry.width, entry.height, false, true);

    // Type 1 sprites key palette index 0 out as the background. Punch the
    // alpha directly rather than using SDL_SetColorKey: LoadBitmapFromChunky
    // hands back a surface that already has a full alpha channel, and a colour
    // key is ignored when converting one of those to a texture, so the key
    // colour would render as an opaque rectangle around the sprite.
    if (entry.type == 1) {
        SDL_Surface* surf = bitmap->GetSurface();
        if (surf) {
            const SrLibColor& key = palette[0];
            SDL_LockSurface(surf);
            uint32_t* pixels = static_cast<uint32_t*>(surf->pixels);
            const uint32_t keyPixel = SDL_MapRGBA(surf->format, key.r, key.g, key.b, 255);
            const uint32_t clear = SDL_MapRGBA(surf->format, key.r, key.g, key.b, 0);
            for (int i = 0; i < entry.width * entry.height; i++) {
                if (pixels[i] == keyPixel) pixels[i] = clear;
            }
            SDL_UnlockSurface(surf);
            bitmap->UpdateFromSurface();
        }
    }

    return bitmap;
}

void AssetManager::InitDosAssets() {
    // The 8 stickers are the Street Rod SE redraws (SRSE/LIB2 shares stock
    // DOS LIB2's 268-entry index, and 186-193 are among the sprites it
    // replaces - see libp.h's SrseLib2Asset). Stock SR1 LIB2 is the fallback
    // for an install that has no SE data disk.
    lib2Data_vec = LoadDosDataFile(gameBasePath / "SRSE" / "LIB2", gameBasePath / "srsepc.zip",
                                   "SRSE/LIB2", "SE decals");
    if (lib2Data_vec.empty()) {
        lib2Data_vec = LoadDosDataFile(gameBasePath / "LIB2", gameBasePath / "sr1pc.zip",
                                       "SR1/LIB2", "DOS decals");
    }
    if (lib2Data_vec.empty()) {
        return;
    }

    dosImageEntries = bp.ReadEntriesDOS(lib2Data_vec);
    LOG_INFO("LIB2: parsed %d entries", dosImageEntries.first);

    const int dosDecalIndices[] = {
        DOS_LIB2_SE_DECAL_1, DOS_LIB2_SE_DECAL_2, DOS_LIB2_SE_DECAL_3, DOS_LIB2_SE_DECAL_4,
        DOS_LIB2_SE_DECAL_5, DOS_LIB2_SE_DECAL_6, DOS_LIB2_SE_DECAL_7, DOS_LIB2_SE_DECAL_8
    };

    for (int i = 0; i < 8; i++) {
        int idx = dosDecalIndices[i];
        auto bitmap = DecodeDosSprite(lib2Data_vec, dosImageEntries, idx);
        if (!bitmap) {
            LOG_INFO("LIB2: decal index %d out of range (%d entries), skipping", idx, dosImageEntries.first);
            dosDecalBitmaps.push_back(nullptr);
            continue;
        }
        const DOSBitmapEntry_t& entry = dosImageEntries.second[idx];
        dosDecalBitmaps.push_back(std::move(bitmap));
        LOG_INFO("LIB2: loaded DOS decal %d (%dx%d)", i, entry.width, entry.height);
    }

    LOG_INFO("Loaded %zu DOS decals from LIB2", dosDecalBitmaps.size());
}

void AssetManager::InitCarSets() {
    const std::vector<CarSetDef>& defs = GetCarSetDefs();
    for (size_t i = 0; i < defs.size(); i++) {
        const CarSetDef& def = defs[i];
        if (def.art == CarSetArt::AmigaLibP) {
            // Comes out of lib.p/SR_data, already loaded.
            carSets[i].enabled = true;
            continue;
        }

        const fs::path zip = gameBasePath / def.zipName;
        CarSetAssets& set = carSets[i];

        set.libData = LoadDosDataFile(gameBasePath / def.looseDir / fs::path(def.libEntry).filename(),
                                      zip, def.libEntry, def.name.c_str());
        if (set.libData.empty()) {
            continue;
        }

        // Stats, sprite numbers and pixel offsets all live in the data file,
        // so the art on its own is not usable.
        set.carData = LoadDosDataFile(gameBasePath / def.looseDir / fs::path(def.dataEntry).filename(),
                                      zip, def.dataEntry, def.name.c_str());
        if (set.carData.empty()) {
            set.libData.clear();
            continue;
        }

        set.imageEntries = bp.ReadEntriesDOS(set.libData);
        LOG_INFO("Car set '%s': %d sprites, %zu bytes of car data",
                 def.id.c_str(), set.imageEntries.first, set.carData.size());
    }
}

bool AssetManager::HasCarSetData(int carSet) const {
    if (carSet < 0 || carSet >= (int)carSets.size()) return false;
    if (GetCarSetDefs()[carSet].art == CarSetArt::AmigaLibP) return true;
    return !carSets[carSet].libData.empty() && !carSets[carSet].carData.empty();
}

const byte_vector& AssetManager::GetCarSetData(int carSet) const {
    static const byte_vector empty;
    if (carSet < 0 || carSet >= (int)carSets.size()) return empty;
    return carSets[carSet].carData;
}

const std::pair<int, std::vector<DOSBitmapEntry_t>>& AssetManager::GetCarSetImageEntries(int carSet) const {
    static const std::pair<int, std::vector<DOSBitmapEntry_t>> empty;
    if (carSet < 0 || carSet >= (int)carSets.size()) return empty;
    return carSets[carSet].imageEntries;
}

bool AssetManager::IsCarSetEnabled(int carSet) const {
    if (carSet < 0 || carSet >= (int)carSets.size()) return false;
    if (GetCarSetDefs()[carSet].mandatory) return true;
    return carSets[carSet].enabled && HasCarSetData(carSet);
}

void AssetManager::SetCarSetEnabled(int carSet, bool enabled) {
    if (carSet < 0 || carSet >= (int)carSets.size()) return;
    if (GetCarSetDefs()[carSet].mandatory) return;
    carSets[carSet].enabled = enabled;
}

Bitmap* AssetManager::GetCarSetSprite(int carSet, int spriteIndex) {
    if (carSet < 0 || carSet >= (int)carSets.size()) return nullptr;
    CarSetAssets& set = carSets[carSet];

    auto cached = set.spriteCache.find(spriteIndex);
    if (cached != set.spriteCache.end()) {
        return cached->second.get();
    }

    auto bitmap = DecodeDosSprite(set.libData, set.imageEntries, spriteIndex);
    if (!bitmap) {
        LOG_INFO("Car set '%s': sprite %d out of range (%d entries)",
                 GetCarSetDefs()[carSet].id.c_str(), spriteIndex, set.imageEntries.first);
        return nullptr;
    }
    const DOSBitmapEntry_t& entry = set.imageEntries.second[spriteIndex];
    bitmap->SetDimensions(entry.width, entry.height);

    Bitmap* raw = bitmap.get();
    set.spriteCache[spriteIndex] = std::move(bitmap);
    return raw;
}

Bitmap* AssetManager::GetCarSetSpriteWithPalette(int carSet, int spriteIndex,
                                                 const std::vector<uint16_t>& carPalette) {
    if (carSet < 0 || carSet >= (int)carSets.size()) return nullptr;
    if (carPalette.empty()) return GetCarSetSprite(carSet, spriteIndex);

    CarSetAssets& set = carSets[carSet];
    AssetPaletteKey cacheKey{spriteIndex, hashPalette(carPalette)};

    auto cached = set.paletteVariantCache.find(cacheKey);
    if (cached != set.paletteVariantCache.end()) {
        return cached->second.get();
    }

    auto bitmap = DecodeDosSprite(set.libData, set.imageEntries, spriteIndex, &carPalette);
    if (!bitmap) {
        LOG_INFO("Car set '%s': sprite %d out of range (%d entries)",
                 GetCarSetDefs()[carSet].id.c_str(), spriteIndex, set.imageEntries.first);
        return nullptr;
    }
    const DOSBitmapEntry_t& entry = set.imageEntries.second[spriteIndex];
    bitmap->SetDimensions(entry.width, entry.height);

    Bitmap* raw = bitmap.get();
    set.paletteVariantCache[cacheKey] = std::move(bitmap);
    return raw;
}

Bitmap* AssetManager::GetDosDecal(int index) {
    if (index < 0 || index >= (int)dosDecalBitmaps.size()) return nullptr;
    return dosDecalBitmaps[index].get();
}

bool AssetManager::HasDosDecals() const {
    return !dosDecalBitmaps.empty();
}

std::vector<AssetManager::GameSearchPath> AssetManager::GameSearchPaths() {
    std::vector<GameSearchPath> out = {
        { fs::current_path(),               "current directory" },
        { fs::current_path() / "..",        "parent directory" },
        { fs::current_path() / ".." / "..", "grandparent directory" },
    };

    // Launched from Finder or a file manager the working directory is not the
    // one the executable sits in, so look around the executable as well.
    // Three levels up from the binary is the folder holding RodLegacy.app
    // (Contents/MacOS -> Contents -> RodLegacy.app -> here); elsewhere that
    // last level is just a harmless miss.
    if (char* base = SDL_GetBasePath()) {
        const fs::path dir(base);
        SDL_free(base);
        out.push_back({ dir.lexically_normal(),                        "executable directory" });
        out.push_back({ (dir / "..").lexically_normal(),               "executable parent" });
        out.push_back({ (dir / ".." / "..").lexically_normal(),        "executable grandparent" });
        out.push_back({ (dir / ".." / ".." / "..").lexically_normal(), "beside the app bundle" });
    }

    if (const char* envPath = getenv("STREETROD_DATA")) {
        out.push_back({ fs::path(envPath), "STREETROD_DATA env var" });
    }
    return out;
}

fs::path AssetManager::FindGameDirectory() {
    // First try loading from saved config
    fs::path savedPath = LoadSavedGamePath();
    if (!savedPath.empty() && ValidateGamePath(savedPath)) {
        LOG_INFO("Game data: %s (saved config)", savedPath.string().c_str());
        return savedPath;
    }

    const std::vector<GameSearchPath> searchPaths = GameSearchPaths();

    for (const auto& entry : searchPaths) {
        if (ValidateGamePath(entry.path)) {
            LOG_INFO("Game data: %s (found via %s)", entry.path.string().c_str(), entry.source);
            SaveGamePath(entry.path);
            return entry.path;
        }
    }

    // If we get here, no valid path was found - now we can log the details
    LOG_WARN("No valid game directory found in search paths:");
    for (const auto& entry : searchPaths) {
        LOG_WARN("  checked %s: %s", entry.source, entry.path.string().c_str());
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

bool AssetManager::ValidateGamePath(const fs::path& path) {
    // Accept raw ADF files side by side
    if (fs::exists(path / "SR1AMIGA_DISK1.adf") && fs::exists(path / "SR1AMIGA_DISK2.adf")) {
        return true;
    }
    // Accept sr1amiga.zip containing both disks
    fs::path zip = path / "sr1amiga.zip";
    if (fs::exists(zip) &&
        ZipContainsFile(zip, "SR1AMIGA_DISK1.adf") &&
        ZipContainsFile(zip, "SR1AMIGA_DISK2.adf")) {
        return true;
    }
    LOG_WARN("ValidateGamePath: no valid game files in %s", path.string().c_str());
    return false;
}

void AssetManager::SetCustomPath(const std::string& customPath) {
    fs::path path = fs::path(customPath);

    if (!ValidateGamePath(path)) {
        throw AssetLoadError("Selected folder does not contain required game files");
    }

    gameBasePath = path;

    LOG_INFO("Using game directory: %s", gameBasePath.string().c_str());

    InitFonts();
    InitGameFiles();
    LoadAmigaFontFromExe();
    InitStockPalettes();
}

fs::path AssetManager::ResolvePath(fs::path relativePath) {
    fs::path fullPath = dataPath / relativePath;
    if (!fs::exists(fullPath)) {
        throw AssetLoadError("Required game file not found: " + fullPath.string());
    }
    return fullPath;
}

// ---------------------------------------------------------------------------
// Unified config.txt  (key=value, one per line)
// ---------------------------------------------------------------------------

fs::path AssetManager::GetPrefDir() {
    char* p = SDL_GetPrefPath("se.grovdata", "rodlegacy");
    if (!p) return {};
    fs::path result = fs::path(p);
    SDL_free(p);
    return result;
}

static fs::path GetConfigPath() {
    fs::path dir = AssetManager::GetPrefDir();
    if (dir.empty()) return {};
    return dir / "config.txt";
}

static std::map<std::string, std::string> ReadConfigFile() {
    std::map<std::string, std::string> kv;
    fs::path p = GetConfigPath();
    if (p.empty() || !fs::exists(p)) return kv;
    std::ifstream f(p);
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return kv;
}

static void WriteConfigFile(const std::map<std::string, std::string>& kv) {
    fs::path p = GetConfigPath();
    if (p.empty()) return;
    std::ofstream f(p, std::ios::out | std::ios::trunc);
    for (const auto& pair : kv)
        f << pair.first << "=" << pair.second << "\n";
    LOG_INFO("Config saved to %s", p.string().c_str());
}

fs::path AssetManager::LoadSavedGamePath() {
    auto kv = ReadConfigFile();
    auto it = kv.find("game_path");
    if (it == kv.end()) return {};
    fs::path path(it->second);
    if (fs::exists(path)) return path;
    return {};
}

fs::path AssetManager::GetSavesDir() {
    fs::path dir = GetPrefDir();
    if (dir.empty()) return {};
    return dir / "saves";
}

void AssetManager::SaveGamePath(const fs::path& path) {
    auto kv = ReadConfigFile();
    kv["game_path"] = path.string();
    WriteConfigFile(kv);
}

static AudioManager::MusicSource DefaultMusicSource(AudioManager* am) {
    if (am->HasAmigaMusic()) return AudioManager::MusicSource::AMIGA;
    if (am->HasPCSpeaker())  return AudioManager::MusicSource::DOS;
    return AudioManager::MusicSource::NONE;
}

static AudioManager::MusicSource ParseMusicSource(const std::string& s, AudioManager* am) {
    if (s == "amiga" && am->HasAmigaMusic()) return AudioManager::MusicSource::AMIGA;
    if (s == "dos"   && am->HasPCSpeaker())  return AudioManager::MusicSource::DOS;
    // "none" may have been saved when no music source was available (e.g. broken zip loading).
    // Fall back to the best available source so the user gets music after a fix.
    return DefaultMusicSource(am);
}

static std::string MusicSourceName(AudioManager::MusicSource src) {
    switch (src) {
        case AudioManager::MusicSource::AMIGA: return "amiga";
        case AudioManager::MusicSource::DOS:   return "dos";
        default:                               return "none";
    }
}

void AssetManager::LoadSettings() {
    auto kv = ReadConfigFile();
    if (kv.empty()) {
        // No saved settings: pick the best available music source and start it.
        if (audioManager) audioManager->SetMusicSource(DefaultMusicSource(audioManager));
        return;
    }
    try {
        auto zoneOverlaysIt = kv.find("debug_show_zone_overlays");
        if (zoneOverlaysIt != kv.end()) debugShowZoneOverlays = std::stoi(zoneOverlaysIt->second) != 0;

        auto hillOcclusionIt = kv.find("hill_occlusion");
        if (hillOcclusionIt != kv.end()) hillOcclusion = std::stoi(hillOcclusionIt->second) != 0;

        auto skipIntroIt = kv.find("skip_intro");
        if (skipIntroIt != kv.end()) skipIntro = std::stoi(skipIntroIt->second) != 0;

        auto extraAnimationsIt = kv.find("extra_animations");
        if (extraAnimationsIt != kv.end()) extraAnimations = std::stoi(extraAnimationsIt->second) != 0;

        auto analogSteeringIt = kv.find("analog_steering");
        if (analogSteeringIt != kv.end()) analogSteering = std::stoi(analogSteeringIt->second) != 0;

        auto puristIt = kv.find("purist_scoring");
        if (puristIt != kv.end()) puristScoring = std::stoi(puristIt->second) != 0;
        // race_sim is standalone (no assets, no SDL), so it holds its own copy.
        srrace::SetPuristScoring(puristScoring);

        // Optional car sets, stored by id ("car_set_srse=1") so adding or
        // reordering sets in the registry cannot flip what is enabled.
        const std::vector<CarSetDef>& defs = GetCarSetDefs();
        for (size_t i = 0; i < defs.size() && i < carSets.size(); i++) {
            if (defs[i].mandatory) continue;
            auto it = kv.find("car_set_" + defs[i].id);
            if (it != kv.end()) carSets[i].enabled = std::stoi(it->second) != 0;
        }

        if (audioManager) {
            auto it = kv.find("sfx_volume");
            if (it != kv.end()) audioManager->SetSfxVolume(std::stoi(it->second));
            it = kv.find("music_volume");
            if (it != kv.end()) audioManager->SetMusicVolume(std::stoi(it->second));
            it = kv.find("music_muted");
            if (it != kv.end()) audioManager->SetMusicMuted(std::stoi(it->second) != 0);
            it = kv.find("music_source");
            AudioManager::MusicSource src = (it != kv.end())
                ? ParseMusicSource(it->second, audioManager)
                : DefaultMusicSource(audioManager);
            audioManager->SetMusicSource(src);
        }
    } catch (...) {}
}

void AssetManager::SaveSettings() {
    auto kv = ReadConfigFile();
    const std::vector<CarSetDef>& carSetDefs = GetCarSetDefs();
    for (size_t i = 0; i < carSetDefs.size() && i < carSets.size(); i++) {
        if (carSetDefs[i].mandatory) continue;
        kv["car_set_" + carSetDefs[i].id] = carSets[i].enabled ? "1" : "0";
    }
    if (audioManager) {
        kv["sfx_volume"]    = std::to_string(audioManager->GetSfxVolume());
        kv["music_volume"]  = std::to_string(audioManager->GetMusicVolume());
        kv["music_muted"]   = audioManager->IsMusicMuted() ? "1" : "0";
        kv["music_source"]  = MusicSourceName(audioManager->GetMusicSource());
    }
    kv["debug_show_zone_overlays"] = debugShowZoneOverlays ? "1" : "0";
    kv["hill_occlusion"] = hillOcclusion ? "1" : "0";
    kv["extra_animations"] = extraAnimations ? "1" : "0";
    kv["skip_intro"] = skipIntro ? "1" : "0";
    kv["analog_steering"] = analogSteering ? "1" : "0";
    kv["purist_scoring"] = puristScoring ? "1" : "0";
    WriteConfigFile(kv);
}
