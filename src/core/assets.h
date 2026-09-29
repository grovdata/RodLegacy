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

struct AssetPaletteKey {
    int assetId;
    size_t paletteHash;
    
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

    static bool ValidateGamePath(const fs::path& path);
    static fs::path FindGameDirectory();
    static void SaveGamePath(const fs::path& path);

    // The config screen scans the same list, so it offers the folder the game would pick.
    struct GameSearchPath {
        fs::path path;
        const char* source;
    };
    static std::vector<GameSearchPath> GameSearchPaths();

    // config.txt, hall_dat and saves live here, so writing works with a read-only game directory.
    static fs::path GetPrefDir();
    static fs::path GetSavesDir();

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

    // The Amiga set is not here; it comes from lib.p/SR_data.
    bool HasCarSetData(int carSet) const;
    Bitmap* GetCarSetSprite(int carSet, int spriteIndex);
    Bitmap* GetCarSetSpriteWithPalette(int carSet, int spriteIndex,
                                       const std::vector<uint16_t>& carPalette);
    const byte_vector& GetCarSetData(int carSet) const;
    const std::pair<int, std::vector<DOSBitmapEntry_t>>& GetCarSetImageEntries(int carSet) const;

    // Persisted by id, so reordering the registry does not change what is enabled.
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

    bool& GetDebugShowZoneOverlaysRef() { return debugShowZoneOverlays; }

    // Deviation, off by default: hills hide the opponent instead of clamping to the ridge. hill_occlusion=1.
    bool& GetHillOcclusionRef() { return hillOcclusion; }

    // Additions the original lacks, off by default. extra_animations=1.
    bool& GetExtraAnimationsRef() { return extraAnimations; }

    // Deviation, off by default: stick deflection sets the steering angle. analog_steering=1.
    bool& GetAnalogSteeringRef() { return analogSteering; }

    // skip_intro=1 or Options -> Settings.
    bool& GetSkipIntroRef() { return skipIntro; }

    // Scores the tune as the 68k does, so top speeds match (best car 122 mph, not 130). purist_scoring=1.
    // race_sim keeps its own copy, updated by LoadSettings and the settings screen.
    bool& GetPuristScoringRef() { return puristScoring; }

    // Deviation, off by default: the race view blends between sim steps. smooth_racing=1.
    bool& GetSmoothRacingRef() { return smoothRacing; }

    // Sim steps a second, 6 (original) to 15. race_speed=N. race_sim keeps its own copy.
    int GetRaceSpeed() const;
    void SetRaceSpeed(int hz);

private:
    bool debugShowZoneOverlays = false;
    bool hillOcclusion = false;
    bool extraAnimations = false;
    bool analogSteering = false;
    bool puristScoring = false;
    bool smoothRacing = false;
    bool skipIntro = false;
    fs::path gameBasePath;
    fs::path dataPath;

    BitmapParser bp;

    std::pair<int, std::vector<BitmapEntry_t>> imageEntries;
    std::vector<std::pair<Asset, std::unique_ptr<Bitmap>>> imageAssets;
    std::map<AssetPaletteKey, std::unique_ptr<Bitmap>> paletteVariantCache;
    std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>> mousePointers;

    AnimatedBitmap* carDecals;

    byte_vector lib2Data_vec;
    std::pair<int, std::vector<DOSBitmapEntry_t>> dosImageEntries;
    std::vector<std::unique_ptr<Bitmap>> dosDecalBitmaps;

    // Parallel to GetCarSetDefs(); the Amiga entry stays empty.
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

    byte_vector disk1Adf_vec;
    byte_vector disk2Adf_vec;
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
    // nullptr if the index is outside the lib. Pass a colour ramp to repaint the body.
    std::unique_ptr<Bitmap> DecodeDosSprite(byte_vector& libData,
        const std::pair<int, std::vector<DOSBitmapEntry_t>>& entries, int index,
        const std::vector<uint16_t>* carPalette = nullptr);
    byte_vector LoadDosDataFile(const fs::path& loosePath, const fs::path& zipPath,
        const std::string& zipEntry, const char* what);
    fs::path ResolvePath(fs::path relativePath);
    void CreateAndCachePaletteVariant(byte_vector& libfile, int index, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey);


    static fs::path LoadSavedGamePath();
};
