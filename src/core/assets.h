#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <tuple>
#include <memory>
#include <map>
#include <filesystem>
#include <stdexcept>

#include "gfx/bitmap.h"
#include "gfx/animated_bitmap.h"
#include "parsers/libp.h"
#include "parsers/carcolor.h"
#include "gfx/SFont.h"
#include "audio/audio_manager.h"

struct Asset {
    LibAssetType type;
    SrLibFile location;
    int id;

    bool operator==(const Asset& other) const {
        return (type == other.type && location == other.location && id == other.id);
    }
};

// Cache key for palette variants: combines asset ID with palette hash
struct AssetPaletteKey {
    int assetId;
    size_t paletteHash;  // Hash of the palette vector for fast comparison
    
    bool operator<(const AssetPaletteKey& other) const {
        if (assetId != other.assetId) return assetId < other.assetId;
        return paletteHash < other.paletteHash;
    }
    
    bool operator==(const AssetPaletteKey& other) const {
        return assetId == other.assetId && paletteHash == other.paletteHash;
    }
};

namespace fs = std::filesystem;

class AssetLoadError : public std::runtime_error {
public:
    AssetLoadError(const std::string& msg) : std::runtime_error(msg) {}
};

typedef std::vector<uint8_t> byte_vector;

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Static methods for game file validation
    static bool ValidateGamePath(const fs::path& path);
    static fs::path FindGameDirectory();
    static void SaveGamePath(const fs::path& path);

    // Everywhere the game data is looked for, in order, with a label for the
    // log. The config screen pre-scans the same list, so the folder it offers
    // is the one the game would have picked on its own.
    struct GameSearchPath {
        fs::path path;
        const char* source;
    };
    static std::vector<GameSearchPath> GameSearchPaths();

    // Per-user preferences directory: config.txt, hall_dat and the saves
    // subdirectory live here, so writing works even when the game directory is
    // read-only or the assets came out of a zip.
    static fs::path GetPrefDir();
    static fs::path GetSavesDir();

    // Instance methods
    void SetCustomPath(const std::string& customPath);
    void ReadAssets();

    void UpdateAsset(byte_vector& libfile, int index);
    void UpdateAsset(byte_vector& libfile, int index, Palette& carPalette);
    void UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette);
    Bitmap* GetBitmapAsset(const Asset& asset);
    Bitmap* GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette);
    const std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>>& GetMousePointers() const {
        return mousePointers;
    }
    Palette GetStockPalette(StockPalette palette);

    std::pair<int, std::vector<BitmapEntry_t>> GetImageEntries() { return imageEntries; }

    AnimatedBitmap* GetCarDecals();

    Bitmap* GetDosDecal(int index);
    bool HasDosDecals() const;

    // Optional car sets loaded from data packs (see car_set.h). The Amiga set
    // is not here - it comes out of the already-loaded lib.p/SR_data.
    // Sprites are decoded on demand and cached, and the Bitmap stays owned by
    // the AssetManager.
    bool HasCarSetData(int carSet) const;
    Bitmap* GetCarSetSprite(int carSet, int spriteIndex);
    // Same, with the car's paint substituted for the stock DOS body colours.
    // Variants are cached per palette, like the Amiga ones.
    Bitmap* GetCarSetSpriteWithPalette(int carSet, int spriteIndex,
                                       const std::vector<uint16_t>& carPalette);
    const byte_vector& GetCarSetData(int carSet) const;
    const std::pair<int, std::vector<DOSBitmapEntry_t>>& GetCarSetImageEntries(int carSet) const;

    // Which optional sets the player has switched on. The mandatory set is
    // always active and is not listed. Persisted in the config file by id, so
    // reordering the registry does not change what is enabled.
    bool IsCarSetEnabled(int carSet) const;
    void SetCarSetEnabled(int carSet, bool enabled);

    void Init(const fs::path& validatedPath, bool muteAudio = false);
    SFont_Font* GetMainFont();
    SFont_Font* GetDebugFont();
    SFont_Font* GetNewspaperFont();

    byte_vector& GetLibPData() { return libPData_vec; }
    byte_vector& GetLibStartData() { return libStartData_vec; }
    byte_vector& GetSrData() { return srData_vec; }
    byte_vector& GetSrExeData() { return srExeData_vec; }
    const byte_vector& GetDosSrExeUnpacked() const { return dosSrExeUnpacked_vec; }

    AudioManager* GetAudioManager() { return audioManager; }
    const fs::path& GetBasePath() const { return gameBasePath; }
    const byte_vector& GetDisk2Adf() const { return disk2Adf_vec; }

    void LoadSettings();
    void SaveSettings();

    // Whether debug builds draw the yellow zone-hitbox overlay. Global across
    // every scene (not per-scene), persisted, defaults off. Returns a
    // reference so the ImGui debug panel can bind its checkbox straight to
    // it, same as other DebugToggle usages.
    bool& GetDebugShowZoneOverlaysRef() { return debugShowZoneOverlays; }

    // Hills hide the opponent instead of the original's clamp-to-the-ridge
    // (Racing::DrawNpcCar). A deviation, so it is off by default and has no UI:
    // set hill_occlusion=1 in config.txt.
    bool& GetHillOcclusionRef() { return hillOcclusion; }

    // Small scene animations the original does not have. Additions, not
    // reconstructions, so they are off by default and have no UI: set
    // extra_animations=1 in config.txt. See RodLegacyResearch/docs/diner.md.
    bool& GetExtraAnimationsRef() { return extraAnimations; }

    // Analog stick steering: how far the stick is pushed sets how far the wheel
    // turns, instead of the original's held-direction ramp to full lock. The
    // keyboard and the D-pad are unaffected either way. A deviation, so it is
    // off by default and has no UI: set analog_steering=1 in config.txt.
    bool& GetAnalogSteeringRef() { return analogSteering; }

    // Start at the name entry instead of the Amiga intro. Off by default;
    // skip_intro=1 in config.txt or Options -> Settings.
    bool& GetSkipIntroRef() { return skipIntro; }

    // Score the engine tune the way the 68k does, (1 - |tune|) * 20/8, so top
    // speeds match the real game (best car 122 mph, not 130). Off by default;
    // set purist_scoring=1 in config.txt. Pushed into the race sim by
    // LoadSettings and by the settings screen, since race_sim keeps its own
    // copy - see RodLegacyResearch/docs/driving/top_speed.md.
    bool& GetPuristScoringRef() { return puristScoring; }

private:
    bool debugShowZoneOverlays = false;
    bool hillOcclusion = false;
    bool extraAnimations = false;
    bool analogSteering = false;
    bool puristScoring = false;
    bool skipIntro = false;
    fs::path gameBasePath;
    fs::path dataPath;

    BitmapParser bp;

    std::pair<int, std::vector<BitmapEntry_t>> imageEntries;
    std::vector<std::pair<Asset, std::unique_ptr<Bitmap>>> imageAssets;  // Base assets (no palette variants)
    std::map<AssetPaletteKey, std::unique_ptr<Bitmap>> paletteVariantCache;  // Cached palette variants
    std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>> mousePointers;

    AnimatedBitmap* carDecals;

    byte_vector lib2Data_vec;
    std::pair<int, std::vector<DOSBitmapEntry_t>> dosImageEntries;
    std::vector<std::unique_ptr<Bitmap>> dosDecalBitmaps;

    // One entry per CarSetDef, parallel to GetCarSetDefs(). The mandatory
    // Amiga set's entry stays empty.
    struct CarSetAssets {
        byte_vector libData;
        byte_vector carData;
        std::pair<int, std::vector<DOSBitmapEntry_t>> imageEntries;
        std::map<int, std::unique_ptr<Bitmap>> spriteCache;
        std::map<AssetPaletteKey, std::unique_ptr<Bitmap>> paletteVariantCache;
        bool enabled = false;
    };
    std::vector<CarSetAssets> carSets;

    SFont_Font* mainFont = nullptr;
    SFont_Font* debugFont = nullptr;
    SFont_Font* newspaperFont = nullptr;

    byte_vector disk1Adf_vec;  // full disk1 ADF image bytes (from file or zip)
    byte_vector disk2Adf_vec;  // full disk2 ADF image bytes (from file or zip)
    byte_vector libStartData_vec;
    byte_vector libPData_vec;
    byte_vector srData_vec;
    byte_vector srExeData_vec;
    byte_vector dosSrExeUnpacked_vec;

    std::map<StockPalette, Palette> stockPalettes;

    AudioManager* audioManager;

    void InitFonts();
    void InitMousePointers();
    void InitGameFiles();
    void LoadAmigaFontFromExe();
    void InitStockPalettes();
    void InitAudio(bool muteAudio = false);
    void InitDosAssets();
    void InitCarSets();
    // Decodes one sprite out of an already-parsed DOS lib. Returns nullptr if
    // the index is outside the lib. Pass a car colour ramp to repaint the
    // body, or nullptr for the sprite's stock colours.
    std::unique_ptr<Bitmap> DecodeDosSprite(byte_vector& libData,
        const std::pair<int, std::vector<DOSBitmapEntry_t>>& entries, int index,
        const std::vector<uint16_t>* carPalette = nullptr);
    // Reads a file that ships either loose under gameBasePath or inside a zip.
    byte_vector LoadDosDataFile(const fs::path& loosePath, const fs::path& zipPath,
        const std::string& zipEntry, const char* what);
    fs::path ResolvePath(fs::path relativePath);
    void CreateAndCachePaletteVariant(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey);


    static fs::path LoadSavedGamePath();
};
