#include "core/log.h"
#include "core/assets.h"
#include "core/game_text.h"

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

static size_t hashPalette(const std::vector<uint16_t>& palette) {
    size_t hash = 0;
    for (uint16_t val : palette) {
        hash ^= std::hash<uint16_t>{}(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

extern "C" {
    #include "embedded_resources.h"
}

AssetManager::AssetManager() : carDecals(nullptr), audioManager(nullptr) {
    // Sized up front: LoadSettings runs before the packs are read.
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
    SFont_Font* amigaFont = LoadMainFontFromExe(Files(game).exe, game == SrGame::SR2);
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
    // SR2 has pointers at different offset
    const bool sr2 = game == SrGame::SR2;
    constexpr size_t kSr2PointerShift = 0x2B04;
    uint16_t* srExeData_uint16 = reinterpret_cast<uint16_t*>(Files(game).exe.data() + (sr2 ? kSr2PointerShift : 0));

    for (int i = 0; i < SrLibAmigaSpriteAssetCount; i++) {
        // SR2's pump slot points at the hand sprite.
        const int infoIndex = (sr2 && i == SrLibAmigaSpriteAssetMousePointerPump)
                                  ? SrLibAmigaSpriteAssetMousePointerHand : i;
        SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[infoIndex];
        std::vector<SrLibColor> buffer(info.width * info.height);
        SrLibAmigaSpriteAsset pointer = (SrLibAmigaSpriteAsset)i;
        SrLibAmigaSpriteReadMousePointer(srExeData_uint16, (SrLibAmigaSpriteAsset)infoIndex, buffer.data());
        std::unique_ptr<Bitmap> mousePointer = std::make_unique<Bitmap>();
        mousePointer->LoadBitmapFromChunky(buffer, info.width, info.height);
        mousePointer->SetDimensions(info.width, info.height);
        mousePointers[pointer] = std::move(mousePointer);
    }
}

// Pulls a named file out of an ADF image already in memory. Throws
// AssetLoadError when the image does not hold it.
static byte_vector ExtractFromAdf(const byte_vector& adf, const char* fileName) {
    AdfFileData* data = extract_adf_file_from_buffer(adf.data(), adf.size(), fileName);
    if (!data) {
        throw AssetLoadError(std::string("Failed to extract ") + fileName + " from ADF");
    }
    byte_vector result(static_cast<uint8_t*>(data->data),
                       static_cast<uint8_t*>(data->data) + data->size);
    free_adf_file_data(data);
    return result;
}

static byte_vector ReadWholeFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    const auto size = f.tellg();
    f.seekg(0);
    byte_vector out(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(out.data()), size);
    return out;
}

AssetManager::GameAssets& AssetManager::Files(SrGame which) {
    return games[which];
}

const AssetManager::GameAssets& AssetManager::Files(SrGame which) const {
    static const GameAssets none;
    auto it = games.find(which);
    return it != games.end() ? it->second : none;
}

bool AssetManager::IsGameLoaded(SrGame which) const {
    return !Files(which).libP.empty();
}

bool AssetManager::LoadGame(SrGame which, std::string* error) {
    if (IsGameLoaded(which)) return true;

    // Both games lay their disks out the same way.
    const SrGameFiles& gf = SrGameGetFiles(which);
    const char* disk1Name = SrLibArchiveNames[gf.disk1];
    const char* disk2Name = SrLibArchiveNames[gf.disk2];
    const fs::path disk1Path = gameBasePath / disk1Name;
    const fs::path disk2Path = gameBasePath / disk2Name;
    const fs::path zipPath   = gameBasePath / gf.zipName;

    auto fail = [&](const std::string& why) {
        LOG_INFO("%s: %s", gf.displayName, why.c_str());
        if (error) *error = std::string(gf.displayName) + ": " + why;
        return false;
    };

    GameAssets ga;
    const bool loose = fs::exists(disk1Path) && fs::exists(disk2Path);
    if (loose) {
        // A bad disk surfaces as unreadable art much later, so check now.
        if (SrLibVerifyChecksum(disk1Path.string().c_str(),
                                SrLibArchiveChecksums[gf.disk1]) != SrLibStatusSuccess) {
            return fail(std::string(disk1Name) + " is corrupted (checksum mismatch)");
        }
        if (SrLibVerifyChecksum(disk2Path.string().c_str(),
                                SrLibArchiveChecksums[gf.disk2]) != SrLibStatusSuccess) {
            return fail(std::string(disk2Name) + " is corrupted (checksum mismatch)");
        }
        LOG_INFO("Loading %s from ADF files in %s", gf.displayName,
                 gameBasePath.string().c_str());
        ga.disk1Adf = ReadWholeFile(disk1Path);
        ga.disk2Adf = ReadWholeFile(disk2Path);
    } else if (fs::exists(zipPath)) {
        LOG_INFO("Loading %s from %s in %s", gf.displayName, gf.zipName,
                 gameBasePath.string().c_str());
        ga.disk1Adf = ZipExtractToMemory(zipPath, disk1Name);
        ga.disk2Adf = ZipExtractToMemory(zipPath, disk2Name);
    } else {
        return fail(std::string("no ") + disk1Name + " / " + disk2Name + " and no " +
                    gf.zipName + " in " + gameBasePath.string());
    }
    if (ga.disk1Adf.empty() || ga.disk2Adf.empty()) {
        return fail("could not read both disk images");
    }

    // The whole images stay resident anyway.
    try {
        ga.exe      = ExtractFromAdf(ga.disk1Adf, "Street_Rod");
        ga.libStart = ExtractFromAdf(ga.disk1Adf, "libstart.p");
        ga.libP     = ExtractFromAdf(ga.disk2Adf, "lib.p");
        ga.data     = ExtractFromAdf(ga.disk2Adf, gf.dataFile);
    } catch (const AssetLoadError& e) {
        return fail(e.what());
    }

    if (which == SrGame::SR2) {
        try {
            ga.srEnd = ExtractFromAdf(ga.disk1Adf, "SR2_end");
        } catch (const AssetLoadError& e) {
            LOG_WARN("%s: %s, no ending", gf.displayName, e.what());
        }
    }

    ga.imageEntries = bp.ReadEntries(ga.libP);
    if (which == SrGame::SR2) {
        ga.palettes = SrLibReadSr2Palettes(ga.exe);
        if (ga.palettes.empty()) {
            LOG_WARN("%s: no palette table in the executable, using Street Rod's",
                     gf.displayName);
        }
    }

    LOG_INFO("Loaded %s: %d lib.p assets", gf.displayName, ga.imageEntries.first);
    games[which] = std::move(ga);
    return true;
}

byte_vector& AssetManager::GetLibPData(SrGame which) { return Files(which).libP; }
byte_vector& AssetManager::GetSrData(SrGame which)  { return Files(which).data; }
byte_vector& AssetManager::GetLibStartData()        { return Files(game).libStart; }
byte_vector& AssetManager::GetSrExeData()           { return Files(game).exe; }
byte_vector& AssetManager::GetSrExeData(SrGame which) { return Files(which).exe; }
const byte_vector& AssetManager::GetDisk2Adf() const { return Files(game).disk2Adf; }

const std::pair<int, std::vector<BitmapEntry_t>>& AssetManager::GetImageEntries(SrGame which) {
    return Files(which).imageEntries;
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
    audioManager->SetSr2Game(game == SrGame::SR2);

    const byte_vector& disk1 = Files(game).disk1Adf;
    if (!audioManager->Initialize(disk1.data(), disk1.size())) {
        LOG_INFO("Warning: Audio initialization failed, continuing without audio");
        delete audioManager;
        audioManager = nullptr;
    } else {
        LOG_INFO("Audio system initialized successfully");

        if (!disk1.empty()) {
            audioManager->InitAmigaMusic(disk1.data(), disk1.size(), game == SrGame::SR2);
        }

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

        // Street Rod 2's own speaker loops, from its SR.EXE: in sr2pc.zip, or loose if that SR.EXE is SR2's.
        if (game == SrGame::SR2) {
            const fs::path sr2Zip = gameBasePath / "sr2pc.zip";
            std::vector<byte_vector> candidates;
            if (fs::exists(sr2Zip)) candidates.push_back(ZipExtractToMemory(sr2Zip, "SR.EXE"));
            if (!srExePacked.empty()) candidates.push_back(srExePacked);
            for (const byte_vector& packed : candidates) {
                if (packed.empty()) continue;
                exepack::UnpackExeResult unpacked = exepack::unpack_exepack_to_exe_bytes(packed.data(), packed.size());
                const byte_vector& exe = unpacked.ok() ? unpacked.exe_bytes : packed;
                std::vector<pcspk::LoopSong> loops = pcspk::ReadLoopsSr2(exe.data(), exe.size());
                if (loops[static_cast<size_t>(pcspk::Loop::IdlePutt)].events.empty()) continue;
                audioManager->SetPcSpeakerLoops(std::move(loops));
                LOG_INFO("PC speaker loops: Street Rod 2's own");
                std::vector<std::vector<SrNote>> songs;
                if (sr2_exe_build_songs(exe.data(), exe.size(), songs)) {
                    std::vector<std::string> names;
                    for (size_t i = 0; i < songs.size(); ++i) names.push_back("Song " + std::to_string(i + 1));
                    audioManager->SetPcSpeakerSongs(std::move(songs), std::move(names));
                }
                break;
            }
        }

        // Street Rod 2's AdLib soundtrack: the MSR.SND instrument bank plus the
        // .MUS songs, loose in the game folder or inside sr2pc.zip.
        fs::path sndPath   = gameBasePath / "MSR.SND";
        fs::path sr2ZipPath = gameBasePath / "sr2pc.zip";
        if (fs::exists(sndPath) || fs::exists(sr2ZipPath)) {
            byte_vector snd = LoadDosDataFile(sndPath, sr2ZipPath, "MSR.SND",
                                              "SR2 AdLib instruments");
            if (!snd.empty() && audioManager->InitAdlibMusic(snd.data(), snd.size())) {
                // The three in-game songs, in the game's own order.
                struct AdlibSong { const char* file; const char* label; };
                static const AdlibSong kSongs[] = {
                    {"MSRBEG.MUS", "Intro"},
                    {"MSR24.MUS",  "Song 1"},
                    {"MSR2.MUS",   "Song 2"},
                };
                auto load = [&](const char* file) {
                    return LoadDosDataFile(gameBasePath / file, sr2ZipPath, file, "SR2 AdLib song");
                };
                int loaded = 0;
                for (const AdlibSong& song : kSongs) {
                    byte_vector mus = load(song.file);
                    if (!mus.empty() &&
                        audioManager->AddAdlibSong(song.label, mus.data(), mus.size())) {
                        loaded++;
                    }
                }
                byte_vector win = load("MSR21.MUS");
                if (!win.empty()) {
                    audioManager->SetAdlibVictorySong("Victory", win.data(), win.size());
                }
                LOG_INFO("AdLib music: %d songs loaded", loaded);
            }
        }

        // The C64 soundtrack: the player PRGs on STREETR1.D64, loose or inside sr1commodore.zip.
        const fs::path d64Path = gameBasePath / "STREETR1.D64";
        const fs::path c64Zip  = gameBasePath / "sr1commodore.zip";
        if (fs::exists(d64Path) || fs::exists(c64Zip)) {
            byte_vector d64 = LoadDosDataFile(d64Path, c64Zip, "STREETR1.D64", "C64 music");
            if (!d64.empty()) audioManager->InitC64Music(d64);
        }
    }
}

void AssetManager::Init(const fs::path& validatedPath, bool muteAudio, SrGame whichGame) {
    try {
        game = whichGame;
        gameBasePath = validatedPath;
        dataPath = gameBasePath;
        InitFonts();
        std::string error;
        if (!LoadGame(game, &error)) throw AssetLoadError(error);
        GameText::Load(Files(game).exe, game == SrGame::SR2);
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
        // The played game's art is decoded up front; another game's waits
        // until something asks for one of its sprites - see GetBitmapAsset.
        GameAssets& played = Files(game);
        for (int i = 0; i < played.imageEntries.first; i++) {
            UpdateAsset(played.libP, i, game);
        }

        // SR2 has no stickers.
        if (carDecals == nullptr && game == SrGame::SR1) {
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

void AssetManager::UpdateAsset(byte_vector& libfile, int index, SrGame fromGame) {
    Palette empty;
    UpdateAsset(libfile, index, empty, fromGame);
}


void AssetManager::UpdateAsset(byte_vector& libfile, int index, Palette& carPalette,
                               SrGame fromGame) {
    const auto& entries = GetImageEntries(fromGame);
    if (index < 0 || index >= (int)entries.second.size()) {
        LOG_ERROR("UpdateAsset: index %d outside lib.p (%zu entries)", index, entries.second.size());
        return;
    }
    BitmapEntry_t b = entries.second[index];
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, entries.first);
    std::vector<uint16_t> palData =
        bp.GetPalette(SrLibFileAmigaLibP, index, fromGame, Files(fromGame).palettes);
    Palette pal = bp.PreparePalette(palData, true);
    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, pal, carPalette, index, fromGame);

    Asset a;
    a.id = index;
    a.location = SrLibFileAmigaLibP;
    a.game = fromGame;
    a.type = bp.GetAssetType(SrLibFileAmigaLibP, index, fromGame);

    bool isMask = a.type == LibAssetMask;
    const bool loadOpaque = bp.LoadsOpaque(SrLibFileAmigaLibP, index, fromGame);

    if (a.type == LibAssetSpritesheet) {
        std::pair<int, int> info = bp.GetSpritesheetInfo(SrLibFileAmigaLibP, index, fromGame);
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


void AssetManager::UpdateAssetWithCarPalette(byte_vector& libfile, int index, StandardCarColor palette,
                                           SrGame fromGame) {
    std::vector<uint16_t> carPalette = GetStandardCarColor(palette);
    Palette newPalette = bp.PrepareCarPalette(carPalette);
    UpdateAsset(libfile, index, newPalette, fromGame);
}

std::vector<uint16_t> AssetManager::GetAssetPalette(const Asset& asset) {
    return bp.GetPalette(asset.location, asset.id, asset.game,
                         Files(asset.game).palettes);
}

Bitmap* AssetManager::GetBitmapAsset(const Asset& asset) {
    for (auto& i : imageAssets) {
        if (i.first == asset) {
            return i.second.get();
        }
    }

    // Only the played game's lib.p is decoded up front.
    if (asset.location == SrLibFileAmigaLibP && IsGameLoaded(asset.game)) {
        UpdateAsset(GetLibPData(asset.game), asset.id, asset.game);
        for (auto& i : imageAssets) {
            if (i.first == asset) return i.second.get();
        }
    }

    LOG_ERROR("Warning: Asset not found (wrong type of asset?): %d", asset.id);
    return nullptr;
}

AnimatedBitmap* AssetManager::GetCarDecals() {
    return this->carDecals;
}

Bitmap* AssetManager::GetBitmapAssetWithPalette(byte_vector& libfile, Asset asset, const std::vector<uint16_t>& carPalette) {
    size_t paletteHash = hashPalette(carPalette);
    AssetPaletteKey cacheKey{asset.id, paletteHash, asset.game};

    auto it = paletteVariantCache.find(cacheKey);
    if (it != paletteVariantCache.end()) {
        return it->second.get();
    }

    CreateAndCachePaletteVariant(libfile, asset, carPalette, cacheKey);
    return paletteVariantCache[cacheKey].get();
}

void AssetManager::CreateAndCachePaletteVariant(byte_vector& libfile, const Asset& asset, const std::vector<uint16_t>& carPalette, const AssetPaletteKey& cacheKey) {
    const int index = asset.id;
    const auto& entries = GetImageEntries(asset.game);
    if (index < 0 || index >= (int)entries.second.size()) {
        LOG_ERROR("CreateAndCachePaletteVariant: index %d outside lib.p (%zu entries)",
                  index, entries.second.size());
        return;
    }
    BitmapEntry_t b = entries.second[index];
    std::vector<uint8_t> data = bp.ReadImage(libfile, b, index, entries.first);
    std::vector<uint16_t> palData =
        bp.GetPalette(asset.location, index, asset.game, Files(asset.game).palettes);

    Palette processedCarPalette = bp.PrepareCarPalette(carPalette);
    Palette basePal = bp.PreparePalette(palData, true);

    std::vector<SrLibColor> chunkyData = bp.PlanarToChunky(data, b, basePal, processedCarPalette, index, asset.game);

    const LibAssetType type = bp.GetAssetType(asset.location, index, asset.game);
    const bool isMask = type == LibAssetMask;
    const bool loadOpaque = bp.LoadsOpaque(asset.location, index, asset.game);

    std::unique_ptr<Bitmap> bitmap = std::make_unique<Bitmap>();
    bitmap->LoadBitmapFromChunky(chunkyData, b.width, b.height, isMask, loadOpaque);
    bitmap->SetDimensions(b.width, b.height);

    // Not in imageAssets, to prevent unbounded growth.
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
    // Opaque, so black pixels are preserved.
    bitmap->LoadBitmapFromChunky(chunkyData, entry.width, entry.height, false, true);

    // Punch alpha directly: a colour key is ignored when converting a surface that already has alpha.
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
    // The stickers are the SE redraws; stock SR1 LIB2 is the fallback without the SE data disk.
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
        if (!CarSetFitsGame(def, game)) continue;   // never mixes backwards
        if (def.zipName.empty()) {
            // Reads a game's own lib.p and data file.
            if (CarSetIsOwnGame(def, game)) carSets[i].enabled = true;
            else LoadGame(def.game);
            continue;
        }

        const fs::path zip = gameBasePath / def.zipName;
        CarSetAssets& set = carSets[i];

        set.libData = LoadDosDataFile(gameBasePath / def.looseDir / fs::path(def.libEntry).filename(),
                                      zip, def.libEntry, def.name.c_str());
        if (set.libData.empty()) {
            continue;
        }

        // Stats, sprite numbers and offsets live in the data file.
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
    const CarSetDef& def = GetCarSetDefs()[carSet];
    // Street Rod 2's cars cannot race in Street Rod, so its set is not offered there.
    if (!CarSetFitsGame(def, game)) return false;
    // A set with no files of its own is there exactly when its game is.
    if (def.zipName.empty()) return IsGameLoaded(def.game);
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
    if (CarSetIsOwnGame(GetCarSetDefs()[carSet], game)) return true;
    return carSets[carSet].enabled && HasCarSetData(carSet);
}

void AssetManager::SetCarSetEnabled(int carSet, bool enabled) {
    if (carSet < 0 || carSet >= (int)carSets.size()) return;
    if (CarSetIsOwnGame(GetCarSetDefs()[carSet], game)) return;
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

    // Launched from Finder the working directory differs; three levels up is the folder holding RodLegacy.app.
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

    LOG_WARN("No valid game directory found in search paths:");
    for (const auto& entry : searchPaths) {
        LOG_WARN("  checked %s: %s", entry.source, entry.path.string().c_str());
    }

    throw AssetLoadError("Could not find game data directory. Please set STREETROD_DATA environment variable.");
}

bool AssetManager::GameAvailable(const fs::path& path, SrGame which) {
    const SrGameFiles& gf = SrGameGetFiles(which);
    const char* disk1 = SrLibArchiveNames[gf.disk1];
    const char* disk2 = SrLibArchiveNames[gf.disk2];
    if (fs::exists(path / disk1) && fs::exists(path / disk2)) {
        return true;
    }
    // Accept the game's zip containing both disks
    const fs::path zip = path / gf.zipName;
    return fs::exists(zip) && ZipContainsFile(zip, disk1) && ZipContainsFile(zip, disk2);
}

bool AssetManager::ValidateGamePath(const fs::path& path) {
    // Either game is playable on its own, so one of them is enough.
    for (SrGame g : {SrGame::SR1, SrGame::SR2}) {
        if (GameAvailable(path, g)) return true;
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
    std::string error;
    if (!LoadGame(game, &error)) throw AssetLoadError(error);
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

fs::path AssetManager::GetPrefDir() {
    char* p = SDL_GetPrefPath("grovdata","rodlegacy");
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

std::string AssetManager::ReadSetting(const std::string& key) {
    auto kv = ReadConfigFile();
    auto it = kv.find(key);
    return it == kv.end() ? std::string() : it->second;
}

void AssetManager::WriteSetting(const std::string& key, const std::string& value) {
    auto kv = ReadConfigFile();
    kv[key] = value;
    WriteConfigFile(kv);
}

void AssetManager::SaveGamePath(const fs::path& path) {
    auto kv = ReadConfigFile();
    kv["game_path"] = path.string();
    WriteConfigFile(kv);
}

static AudioManager::MusicSource DefaultMusicSource(AudioManager* am) {
    if (am->HasAmigaMusic()) return AudioManager::MusicSource::AMIGA;
    if (am->HasPCSpeaker())  return AudioManager::MusicSource::DOS;
    if (am->HasAdlibMusic()) return AudioManager::MusicSource::ADLIB;
    if (am->HasC64Music())   return AudioManager::MusicSource::C64;
    return AudioManager::MusicSource::NONE;
}

static AudioManager::MusicSource ParseMusicSource(const std::string& s, AudioManager* am) {
    if (s == "amiga" && am->HasAmigaMusic()) return AudioManager::MusicSource::AMIGA;
    if (s == "dos"   && am->HasPCSpeaker())  return AudioManager::MusicSource::DOS;
    if (s == "adlib" && am->HasAdlibMusic()) return AudioManager::MusicSource::ADLIB;
    if (s == "c64"   && am->HasC64Music())   return AudioManager::MusicSource::C64;
    return DefaultMusicSource(am);
}

// config.txt's `engine_source`: "amiga" for Street Rod's looped sample, "adlib"
// for Street Rod 2's DOS OPL engine, "pcspeaker" for Street Rod's DOS speaker note. Anything else keeps the Amiga one.
static AudioManager::EngineSource ParseEngineSource(const std::string& s) {
    if (s == "adlib") return AudioManager::EngineSource::ADLIB;
    if (s == "pcspeaker") return AudioManager::EngineSource::PCSPEAKER;
    if (s == "c64") return AudioManager::EngineSource::C64;
    return AudioManager::EngineSource::AMIGA;
}

static std::string EngineSourceName(AudioManager::EngineSource src) {
    switch (src) {
        case AudioManager::EngineSource::ADLIB:     return "adlib";
        case AudioManager::EngineSource::PCSPEAKER: return "pcspeaker";
        case AudioManager::EngineSource::C64:       return "c64";
        default:                                    return "amiga";
    }
}

static std::string MusicSourceName(AudioManager::MusicSource src) {
    switch (src) {
        case AudioManager::MusicSource::AMIGA: return "amiga";
        case AudioManager::MusicSource::DOS:   return "dos";
        case AudioManager::MusicSource::ADLIB: return "adlib";
        case AudioManager::MusicSource::C64:   return "c64";
        default:                               return "none";
    }
}

void AssetManager::LoadSettings() {
    auto kv = ReadConfigFile();
    if (kv.empty()) {
        // The intro starts the music itself, partway in.
        if (audioManager) audioManager->SetMusicSource(DefaultMusicSource(audioManager), skipIntro);
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

        auto betterClickboxesIt = kv.find("better_clickboxes");
        if (betterClickboxesIt != kv.end()) betterClickboxes = std::stoi(betterClickboxesIt->second) != 0;

        auto analogSteeringIt = kv.find("analog_steering");
        if (analogSteeringIt != kv.end()) analogSteering = std::stoi(analogSteeringIt->second) != 0;

        auto uncapIt = kv.find("uncap_top_speed");
        if (uncapIt != kv.end()) uncapTopSpeed = std::stoi(uncapIt->second) != 0;
        // race_sim holds its own copy.
        srrace::SetUncapTopSpeed(uncapTopSpeed);

        auto sr2FixIt = kv.find("sr2_fix_bugs");
        if (sr2FixIt != kv.end()) sr2FixBugs = std::stoi(sr2FixIt->second) != 0;
        srrace::SetSr2BugFixes(sr2FixBugs);

        auto smoothIt = kv.find("smooth_racing");
        if (smoothIt != kv.end()) smoothRacing = std::stoi(smoothIt->second) != 0;

        auto recordScaleIt = kv.find("record_scale");
        if (recordScaleIt != kv.end()) recordScale = std::clamp(std::stoi(recordScaleIt->second), 1, 4);

        auto speedIt = kv.find("race_speed");
        if (speedIt != kv.end()) srrace::SetSimHz(std::stoi(speedIt->second));

        // Stored by id, so reordering the registry cannot flip what is enabled.
        const std::vector<CarSetDef>& defs = GetCarSetDefs();
        // The played game's own set is not optional, so it has no key.
        for (size_t i = 0; i < defs.size() && i < carSets.size(); i++) {
            if (CarSetIsOwnGame(defs[i], game)) continue;
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
            audioManager->SetMusicSource(src, skipIntro);
            it = kv.find("engine_source");
            if (it != kv.end()) {
                audioManager->SetEngineSource(ParseEngineSource(it->second));
            }
            it = kv.find("sfx_source");
            if (it != kv.end() && it->second == "pcspeaker") {
                audioManager->SetSfxSource(AudioManager::SfxSource::PCSPEAKER);
            } else if (it != kv.end() && it->second == "adlib") {
                audioManager->SetSfxSource(AudioManager::SfxSource::ADLIB);
            } else if (it != kv.end() && it->second == "c64") {
                audioManager->SetSfxSource(AudioManager::SfxSource::C64);
            }
        }
    } catch (...) {}
}

int AssetManager::GetRaceSpeed() const {
    return srrace::SimHz();
}

void AssetManager::SetRaceSpeed(int hz) {
    srrace::SetSimHz(hz);
}

void AssetManager::SaveSettings() {
    auto kv = ReadConfigFile();
    const std::vector<CarSetDef>& carSetDefs = GetCarSetDefs();
    for (size_t i = 0; i < carSetDefs.size() && i < carSets.size(); i++) {
        if (CarSetIsOwnGame(carSetDefs[i], game)) continue;
        kv["car_set_" + carSetDefs[i].id] = carSets[i].enabled ? "1" : "0";
    }
    if (audioManager) {
        kv["sfx_volume"]    = std::to_string(audioManager->GetSfxVolume());
        kv["music_volume"]  = std::to_string(audioManager->GetMusicVolume());
        kv["music_muted"]   = audioManager->IsMusicMuted() ? "1" : "0";
        kv["music_source"]  = MusicSourceName(audioManager->GetMusicSource());
        kv["engine_source"] = EngineSourceName(audioManager->GetEngineSource());
        kv["sfx_source"]    = audioManager->GetSfxSource() == AudioManager::SfxSource::PCSPEAKER ? "pcspeaker"
                            : audioManager->GetSfxSource() == AudioManager::SfxSource::ADLIB   ? "adlib"
                            : audioManager->GetSfxSource() == AudioManager::SfxSource::C64     ? "c64"
                                                                                                : "amiga";
    }
    kv["debug_show_zone_overlays"] = debugShowZoneOverlays ? "1" : "0";
    kv["hill_occlusion"] = hillOcclusion ? "1" : "0";
    kv["extra_animations"] = extraAnimations ? "1" : "0";
    kv["better_clickboxes"] = betterClickboxes ? "1" : "0";
    kv["skip_intro"] = skipIntro ? "1" : "0";
    kv["analog_steering"] = analogSteering ? "1" : "0";
    kv["uncap_top_speed"] = uncapTopSpeed ? "1" : "0";
    kv["sr2_fix_bugs"] = sr2FixBugs ? "1" : "0";
    kv["smooth_racing"] = smoothRacing ? "1" : "0";
    kv["race_speed"] = std::to_string(srrace::SimHz());
    WriteConfigFile(kv);
}
