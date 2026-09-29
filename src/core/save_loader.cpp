#include "core/save_loader.h"
#include <fstream>

namespace {

uint16_t ReadU16BE(const std::vector<uint8_t>& d, size_t off) {
    return static_cast<uint16_t>((d[off] << 8) | d[off + 1]);
}

uint32_t ReadU32BE(const std::vector<uint8_t>& d, size_t off) {
    return (static_cast<uint32_t>(d[off]) << 24) | (static_cast<uint32_t>(d[off + 1]) << 16) |
           (static_cast<uint32_t>(d[off + 2]) << 8) | static_cast<uint32_t>(d[off + 3]);
}

// Only bytes 0-5 of each 10-byte record are real; 6-9 are stale pointers.
std::vector<LoadedPartRecord> ReadPartList(const std::vector<uint8_t>& d, size_t& pos) {
    std::vector<LoadedPartRecord> entries;
    while (true) {
        if (pos + 10 > d.size()) throw SaveLoadError("part list ran past end of file");
        LoadedPartRecord rec;
        rec.value = ReadU16BE(d, pos);
        rec.wearRaw = ReadU16BE(d, pos + 2);
        rec.variant = d[pos + 5];
        uint32_t nextPtr = ReadU32BE(d, pos + 6);
        entries.push_back(rec);
        pos += 10;
        if (nextPtr == 0) break;
    }
    return entries;
}

LoadedCarRecord ReadCarRecord(const std::vector<uint8_t>& d, size_t& pos) {
    if (pos + 52 > d.size()) throw SaveLoadError("car record ran past end of file");
    LoadedCarRecord car;
    car.value = ReadU16BE(d, pos + 0);
    car.specIndex = d[pos + 3];
    car.colorIndex = d[pos + 4];
    for (int i = 0; i < 6; ++i) car.tuningScrews[i] = d[pos + 0x22 + i];
    car.wireState = d[pos + 0x2A];
    car.tuningKnob = static_cast<int8_t>(d[pos + 0x2B]);
    car.fuelTenths = d[pos + 0x2D];
    car.bodyByte = d[pos + 0x2E];

    static constexpr size_t kSublistPtrOffsets[7] = {0x06, 0x0A, 0x0E, 0x12, 0x16, 0x1A, 0x1E};
    uint32_t sublistPtrs[7];
    for (int i = 0; i < 7; ++i) sublistPtrs[i] = ReadU32BE(d, pos + kSublistPtrOffsets[i]);

    pos += 52;
    for (int i = 0; i < 7; ++i) {
        if (sublistPtrs[i] != 0) {
            car.sublists[i] = ReadPartList(d, pos);
        }
    }
    return car;
}

}

LoadedSave SaveGameLoader::Load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) throw SaveLoadError("could not open save file: " + path.string());

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return Parse(data);
}

LoadedSave SaveGameLoader::Parse(const std::vector<uint8_t>& data) {
    if (data.size() < 46 + 190) throw SaveLoadError("file too small to be a valid save");

    LoadedSave save;

    size_t nameEnd = 0;
    while (nameEnd < 14 && data[nameEnd] != 0) ++nameEnd;
    save.name.assign(reinterpret_cast<const char*>(data.data()), nameEnd);
    save.bankroll = static_cast<int>(ReadU32BE(data, 0x10));
    save.ownedCarCount = ReadU16BE(data, 0x22);

    size_t pos = 46;
    if (save.ownedCarCount >= 1) {
        save.hasActiveCar = true;
        save.activeCar = ReadCarRecord(data, pos);
    }

    int rosterCount = save.ownedCarCount >= 1 ? save.ownedCarCount - 1 : 0;
    save.roster.reserve(rosterCount);
    for (int i = 0; i < rosterCount; ++i) {
        save.roster.push_back(ReadCarRecord(data, pos));
    }

    if (pos < data.size() - 190) {
        save.spareParts = ReadPartList(data, pos);
    }

    size_t trailerStart = data.size() - 190;
    // Reputation goes negative on a losing streak.
    save.racesCompleted = static_cast<int16_t>(ReadU16BE(data, trailerStart + 0));
    save.roadRep        = static_cast<int16_t>(ReadU16BE(data, trailerStart + 2));
    save.dragRep        = static_cast<int16_t>(ReadU16BE(data, trailerStart + 4));
    save.lastRaceWon    = ReadU16BE(data, trailerStart + 6) != 0;
    for (size_t i = 0; i < 22; ++i) {
        save.opponentCarSpec[i] = data[trailerStart + 8 + i * 8 + 1];
    }
    save.elapsedTicks = ReadU32BE(data, trailerStart + 184);
    // The road-course template index, fixed for the profile's life.
    save.trackVariant = static_cast<int16_t>(ReadU16BE(data, trailerStart + 188));

    return save;
}
