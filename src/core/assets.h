/*
 * RodLegacy - Asset loading: AssetManager reads the disk images and zips, caches bitmaps, fonts, audio and
 * data, and owns config.txt.
 *
 * Created: 2024-07-11
 * SPDX-License-Identifier: GPL-3.0-or-later (see LICENSE)
 */

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
    // Both games ship a lib.p and both number it from zero, so the id is only
    // meaningful together with the game it came from.
    SrGame game = SrGame::SR1;

    bool operator==(const Asset& other) const {
        return (type == other.type && location == other.location &&
                id == other.id && game == other.game);
    }
};

struct AssetPaletteKey {
    int assetId;
    size_t paletteHash;
    // Both games number their own lib.p from zero, so the id alone is not a
    // key once art from more than one of them is loaded.
    SrGame game = SrGame::SR1;

    bool operator<(const AssetPaletteKey& other) const {
        if (assetId != other.assetId) return assetId < other.assetId;
        if (game != other.game) return game < other.game;
        return paletteHash < other.paletteHash;
    }

    bool operator==(const AssetPaletteKey& other) const {
        return assetId == other.assetId && paletteHash == other.paletteHash &&
               game == other.game;
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

    // True when path holds that game's disks, loose or inside its zip.
    static bool GameAvailable(const fs::path& path, SrGame which);
    // True when path holds either game.
    static bool ValidateGamePath(const fs::path& path);
    static fs::path FindGameDirectory();
    static void SaveGamePath(const fs::path& path);
    // One config.txt key, for settings needed before the assets load (the window). Empty when unset.
    static std::string ReadSetting(const std::string& key);
    static void WriteSetting(const std::string& key, const std::string& value);

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

    void UpdateAsset(byte_vector& libfile, int index,
                     SrGame fromGame = SrGame::SR1);
    void UpdateAsset(byte_vector& libfile, int index, Palette& carPalette,
                     SrGame fromGame = SrGame::SR1);
    void UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette,
                                   SrGame fromGame = SrGame::SR1);
    Bitmap* GetBitmapAsset(const Asset& asset);
    // The palette an asset decodes under, for code that plots raw colour
    // indices instead of blitting a bitmap - the Aqueduct's star field does.
    std::vector<uint16_t> GetAssetPalette(const Asset& asset);
    Bitmap* GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette);
    const std::map<SrLibAmigaSpriteAsset, std::unique_ptr<Bitmap>>& GetMousePointers() const {
        return mousePointers;
    }
    Palette GetStockPalette(StockPalette palette);

    const std::pair<int, std::vector<BitmapEntry_t>>& GetImageEntries() { return GetImageEntries(game); }

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

    void Init(const fs::path& validatedPath, bool muteAudio = false,
              SrGame whichGame = SrGame::SR1);

    // Which game is being played. Fixed at Init; the others are only sources
    // of cars and drivers.
    SrGame GetGame() const { return game; }

    byte_vector& GetSr2End() { return Files(game).srEnd; }
    const std::vector<std::vector<uint16_t>>& GetGamePalettes() { return Files(game).palettes; }

    // Reads a game's disks into memory.
    bool LoadGame(SrGame which, std::string* error = nullptr);
    bool IsGameLoaded(SrGame which) const;

    // A loaded game's files.
    byte_vector& GetLibPData(SrGame which);
    byte_vector& GetSrData(SrGame which);
    const std::pair<int, std::vector<BitmapEntry_t>>& GetImageEntries(SrGame which);
    SFont_Font* GetMainFont();
    SFont_Font* GetDebugFont();
    SFont_Font* GetNewspaperFont();

    byte_vector& GetLibPData() { return GetLibPData(game); }
    byte_vector& GetSrData() { return GetSrData(game); }
    byte_vector& GetLibStartData();
    byte_vector& GetSrExeData();
    // Another game's executable, for tables only it carries.
    byte_vector& GetSrExeData(SrGame which);
    const byte_vector& GetDosSrExeUnpacked() const { return dosSrExeUnpacked_vec; }

    AudioManager* GetAudioManager() { return audioManager; }
    const fs::path& GetBasePath() const { return gameBasePath; }
    const byte_vector& GetDisk2Adf() const;

    void LoadSettings();
    void SaveSettings();

    bool& GetDebugShowZoneOverlaysRef() { return debugShowZoneOverlays; }

    // Deviation, off by default: hills hide the opponent instead of clamping to the ridge. hill_occlusion=1.
    bool& GetHillOcclusionRef() { return hillOcclusion; }

    // Additions the original lacks, off by default. extra_animations=1.
    bool& GetExtraAnimationsRef() { return extraAnimations; }

    // Deviation, off by default: wider hotspots than the original. better_clickboxes=1.
    bool& GetBetterClickboxesRef() { return betterClickboxes; }

    // Deviation, off by default: stick deflection sets the steering angle. analog_steering=1.
    bool& GetAnalogSteeringRef() { return analogSteering; }

    // skip_intro=1 or Options -> Settings.
    bool& GetSkipIntroRef() { return skipIntro; }

    // Off: tune scored as the 68k does (best car 122 mph). On: best car reaches 130. uncap_top_speed=1.
    // race_sim keeps its own copy, updated by LoadSettings and the settings screen.
    bool& GetUncapTopSpeedRef() { return uncapTopSpeed; }

    // Deviation, off by default: the PC build's SR2 part effects and tune. sr2_fix_bugs=1.
    bool& GetSr2FixBugsRef() { return sr2FixBugs; }

    // Deviation, off by default: the race view blends between sim steps. smooth_racing=1.
    bool& GetSmoothRacingRef() { return smoothRacing; }

    // Sim steps a second, 6 (original) to 15. race_speed=N. race_sim keeps its own copy.
    int GetRaceSpeed() const;
    void SetRaceSpeed(int hz);

    // F6 recordings are this many times 320x200, 1 to 4. record_scale=N, config file only.
    int GetRecordScale() const { return recordScale; }

private:
    bool debugShowZoneOverlays = false;
    bool hillOcclusion = false;
    bool extraAnimations = false;
    bool betterClickboxes = false;
    bool analogSteering = false;
    bool uncapTopSpeed = false;
    bool sr2FixBugs = false;
    bool smoothRacing = false;
    bool skipIntro = false;
    int recordScale = 2;
    fs::path gameBasePath;
    SrGame   game = SrGame::SR1;
    fs::path dataPath;

    BitmapParser bp;

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

    // What one game's two disks hold. Both games ship the same files under the
    // same names; only the data file on disk 2 is named differently.
    struct GameAssets {
        byte_vector disk1Adf;    // whole image: the audio player reads it directly
        byte_vector disk2Adf;    // whole image: it carries the sample .sav files
        byte_vector exe;         // Street_Rod
        byte_vector libStart;    // libstart.p
        byte_vector libP;        // lib.p
        byte_vector data;        // SR_Data / SR2_data
        byte_vector srEnd;       // SR2_end, Street Rod 2 only
        std::pair<int, std::vector<BitmapEntry_t>> imageEntries;
        // Street Rod 2 reads its palettes out of its executable; Street Rod's
        // are compiled into the parser, so this stays empty for it.
        std::vector<std::vector<uint16_t>> palettes;
    };
    // Every game whose disks have been read.
    std::map<SrGame, GameAssets> games;

    // One game's files, creating an empty record for a game that was never
    // loaded. IsGameLoaded is what distinguishes the two.
    GameAssets& Files(SrGame which);
    const GameAssets& Files(SrGame which) const;

    byte_vector dosSrExeUnpacked_vec;

    std::map<StockPalette, Palette> stockPalettes;

    AudioManager* audioManager;

    void InitFonts();
    void InitMousePointers();
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
    void CreateAndCachePaletteVariant(byte_vector& libfile, const Asset& asset, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey);


    static fs::path LoadSavedGamePath();
};
