#include "parsers/libp.h"



#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <cmath>
#include <algorithm>
#include <map>

#include "parsers/carcolor.h"
#include "car/car_set.h"

static void WriteUint16BE(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back((value >> 8) & 0xFF);
    out.push_back(value & 0xFF);
}

static void WriteUint32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back((value >> 24) & 0xFF);
    out.push_back((value >> 16) & 0xFF);
    out.push_back((value >> 8) & 0xFF);
    out.push_back(value & 0xFF);
}

static void WriteBitmapEntryBE(std::vector<uint8_t>& out, const BitmapEntry_t& entry) {
    WriteUint16BE(out, entry.width);
    WriteUint16BE(out, entry.height);
    out.push_back(entry.depth);
    out.push_back(entry.unknown1);
    out.insert(out.end(), entry.rleDictionary, entry.rleDictionary + sizeof(entry.rleDictionary));
    WriteUint32BE(out, entry.offset);
    WriteUint16BE(out, entry.sizePacked);
}

static size_t ExpectedPlanarSize(const BitmapEntry_t& entry) {
    return ((size_t)entry.width * (size_t)entry.height * (size_t)entry.depth) / 8;
}

static bool SameColor(const SrLibColor& a, const SrLibColor& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}


const char* SrLibCarDecalNames[16] = {
    "Hot Lips", "Ranger", "Not Bullet Proof", "Hot Rod",
    "Destiny Can Wait", "Love Bandit", "Big Chief", "I'm Not A Cop",
    "USA 1", "Eagle", "Not 4 Sale", "Veteran",
    "Hell", "Hawaii", "Banana Club", "Explosive"
};

// Indexed by SrLibFile.
const char *SrLibArchiveNames[] = {
    "SR1AMIGA_DISK1.adf",
    "SR1AMIGA_DISK2.adf",
    "SR2AMIGA_DISK1.adf",
    "SR2AMIGA_DISK2.adf",
};

const SrGameFiles& SrGameGetFiles(SrGame game) {
    static const SrGameFiles files[] = {
        {SrLibArchiveSR1AmigaDisk1, SrLibArchiveSR1AmigaDisk2,
         "sr1amiga.zip", "SR_Data",  "Street Rod"},
        {SrLibArchiveSR2AmigaDisk1, SrLibArchiveSR2AmigaDisk2,
         "sr2amiga.zip", "SR2_data", "Street Rod 2"},
    };
    return files[static_cast<int>(game)];
}

const uint32_t SrLibArchiveChecksums[] = {
    0x53587B9F,
    0xFF24276C,
    0x5458C0BC,
    0x135549EB
};

void SrLibDebugBitmapEntry(int number, BitmapEntry_t entry) {
    printf("---------\n");
    printf("bitmap %d\n", number);
    printf("width %d\n", entry.width);
    printf("height %d\n", entry.height);
    printf("depth %d\n", entry.depth);
    printf("unknown1 %d\n", entry.unknown1);
    printf("offset %d\n", entry.offset);
    printf("sizePacked %d\n", entry.sizePacked);
}

void SrLibDebugBitplanes(const uint8_t* data, int width, int height, int depth) {
    const int bytesPerColumn = 4;
    const int maxColumns = 8;
    const int totalBytes = width * height * depth;

    for (int i = 0; i < totalBytes; i++) {
        printf("%02x", data[i]);

        if ((i + 1) % bytesPerColumn == 0) {
            printf(" ");
        }

        if ((i + 1) % (bytesPerColumn * maxColumns) == 0) {
            printf("\n");
        }
    }

    if (totalBytes % (bytesPerColumn * maxColumns) != 0) {
        printf("\n");
    }
}


std::pair<int, std::vector<BitmapEntry_t>> BitmapParser::ReadEntries(const std::vector<uint8_t>& data) {
    std::vector<BitmapEntry_t> result;
    VectorReader reader(data);

    uint16_t totalEntriesCount = reader.ReadUint16BE();

    for (int i = 0; i < totalEntriesCount; ++i) {
        BitmapEntry_t newEntry = {};
        newEntry.width = reader.ReadUint16BE();
        newEntry.height = reader.ReadUint16BE();
        newEntry.depth = reader.ReadUint8();
        newEntry.unknown1 = reader.ReadUint8();
        auto dictData = reader.ReadBytes(sizeof(newEntry.rleDictionary));
        memcpy(newEntry.rleDictionary, dictData.data(), sizeof(newEntry.rleDictionary));
        newEntry.offset = reader.ReadUint32BE();
        newEntry.sizePacked = reader.ReadUint16BE();

        result.push_back(newEntry);
    }

    return std::pair<int, std::vector<BitmapEntry_t>>(totalEntriesCount, result);
}

std::vector<uint8_t> BitmapParser::GetPackedData(const std::vector<uint8_t>& data, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    size_t headerSize = 2 + (sizeof(BitmapEntry_t) * (size_t)totalEntriesCount);
    size_t fileOffset = headerSize + (size_t)bitmapEntry.offset;

    if (fileOffset > data.size() || (size_t)bitmapEntry.sizePacked > data.size() - fileOffset) {
        throw std::runtime_error("Can't read data because offset is larger than data size.");
    }

    return std::vector<uint8_t>(
        data.begin() + fileOffset,
        data.begin() + fileOffset + bitmapEntry.sizePacked
    );
}

std::vector<uint8_t> BitmapParser::PackData(const BitmapEntry_t& bitmapEntry, const std::vector<uint8_t>& planarData) {
    if (planarData.size() != ExpectedPlanarSize(bitmapEntry)) {
        throw std::runtime_error("Planar data size does not match bitmap dimensions.");
    }

    std::vector<uint8_t> packed;
    for (size_t i = 0; i < planarData.size(); ) {
        uint8_t value = planarData[i];

        if (value == 0x00 || value == 0xFF) {
            size_t run = 1;
            while (i + run < planarData.size() && planarData[i + run] == value && run < 256) {
                run++;
            }
            packed.push_back(value);
            packed.push_back((uint8_t)(run - 1));
            i += run;
        } else {
            packed.push_back(value);
            i++;
        }
    }

    return packed;
}

std::vector<uint8_t> BitmapParser::BuildAmigaLibP(const std::vector<uint8_t>& originalData,
    const std::map<int, std::vector<uint8_t>>& replacementPlanarData) {
    auto [count, entries] = ReadEntries(originalData);
    if (count <= 0 || entries.empty()) {
        throw std::runtime_error("Cannot rebuild empty or invalid Amiga bitmap library.");
    }

    std::vector<BitmapEntry_t> newEntries = entries;
    std::vector<std::vector<uint8_t>> payloads(entries.size());
    uint32_t currentOffset = 0;

    for (int i = 0; i < count; ++i) {
        auto replacement = replacementPlanarData.find(i);
        if (replacement != replacementPlanarData.end()) {
            if (replacement->second.size() != ExpectedPlanarSize(entries[i])) {
                throw std::runtime_error("Replacement planar data size does not match bitmap entry.");
            }

            memset(newEntries[i].rleDictionary, 0, sizeof(newEntries[i].rleDictionary));
            payloads[i] = PackData(newEntries[i], replacement->second);
        } else {
            payloads[i] = GetPackedData(originalData, entries[i], count);
        }

        if (payloads[i].size() > UINT16_MAX) {
            throw std::runtime_error("Packed bitmap data is too large for Amiga bitmap record.");
        }
        newEntries[i].offset = currentOffset;
        newEntries[i].sizePacked = (uint16_t)payloads[i].size();
        currentOffset += (uint32_t)payloads[i].size();
    }

    std::vector<uint8_t> out;
    out.reserve(2 + newEntries.size() * sizeof(BitmapEntry_t) + currentOffset);
    WriteUint16BE(out, (uint16_t)count);
    for (const auto& entry : newEntries) {
        WriteBitmapEntryBE(out, entry);
    }
    for (const auto& payload : payloads) {
        out.insert(out.end(), payload.begin(), payload.end());
    }

    return out;
}

std::vector<uint8_t> BitmapParser::UnpackData(const std::vector<std::pair<int, uint8_t>>& rle, const BitmapEntry_t& sprite, const std::vector<uint8_t>& data) {
    int p = 0;
    int width = sprite.width / 8;
    int height = sprite.height;
    int depth = sprite.depth;
    size_t size_expected = (sprite.width * sprite.height * sprite.depth) / 8;

    std::vector<uint8_t> flat_bp;

    int pos = 0;

    if (rle.size() != 16) {
        std::cout << "No valid RLE table!" << std::endl;
        return flat_bp;
    }

    while (pos < width * height * depth) {
        if (p >= (int)data.size()) break;
        uint8_t b = data[p];

        bool found = false;
        const uint8_t* directory = sprite.rleDictionary;

        for (int i = 0; i < 16; i++) {
            if (directory[i] == 0) {
                break;
            }
            if (b == directory[i]) {
                for (int c = 0; c < rle[i].first; c++) {
                    pos++;
                    flat_bp.push_back(rle[i].second);
                }
                found = true;
                break;
            }
        }

        if (found) {
            p++;
            continue;
        }

        if (b == 0 || b == 0xFF) {
            if (p + 1 >= (int)data.size()) break;
            uint8_t c = data[p + 1] & 0xFF;
            for (int i = 0; i < c + 1; i++) {
                flat_bp.push_back(b);
                pos++;
            }
            p += 2;
        } else {
            flat_bp.push_back(b);
            pos++;
            p++;
        }
    }

    if (flat_bp.size() < size_expected) {
        int missing = size_expected - flat_bp.size();
        flat_bp.insert(flat_bp.end(), missing, 0x0);
    }

    return flat_bp;
}

std::vector<uint8_t> BitmapParser::PlanarToIndices(const std::vector<uint8_t>& data, BitmapEntry_t entry) {
    std::vector<uint8_t> out(entry.width * entry.height, 0);
    const int planeSize = (entry.width / 8) * entry.height;
    for (int i = 0; i < entry.width * entry.height; ++i) {
        const uint8_t bit = 0x80 >> (i % 8);
        for (int p = 0; p < entry.depth; ++p) {
            if (data[p * planeSize + i / 8] & bit) {
                out[i] |= 1 << p;
            }
        }
    }
    return out;
}

std::vector<SrLibColor> BitmapParser::PlanarToChunky(const std::vector<uint8_t>& data,
    BitmapEntry_t entry, const Palette& palette, const Palette& carPalette, int assetIndex, SrGame game) {
    std::vector<SrLibColor> chunkyData(entry.width * entry.height);

    int planeSize = (entry.width / 8) * entry.height;

    // RACING_INTERIOR_1, RACING_HOOD and RACING_CRASHED_INTERIOR: only their magenta placeholders take the car palette.
    bool useMagentaDetection = game == SrGame::SR2
        ? (assetIndex == SR2_INTERIOR_0 || assetIndex == SR2_INTERIOR_1 || assetIndex == SR2_INTERIOR_2 ||
           assetIndex == SR2_HOOD_TALL || assetIndex == SR2_HOOD_LOW)
        : (assetIndex == 3 || assetIndex == 11 || assetIndex == 19);

    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; x += 8) {
            int offset = (y * entry.width + x) / 8;
            for (int i = 0; i < 8; ++i) {
                uint8_t value = 0;
                uint8_t shift = (1 << (7 - i));

                for (int p = 0; p < entry.depth; ++p) {
                    if (shift & data[p * planeSize + offset]) {
                        value |= 1 << p;
                    }
                }

                bool useLowerRange = value >= 1 && value <= 9;
                bool useUpperRange = value >= 33 && value <= 41;
                bool useCarPalette = false;

                if (!carPalette.empty() && (useLowerRange || useUpperRange)) {
                    if (useMagentaDetection && value < palette.size()) {
                        const SrLibColor& baseColor = palette[value];
                        // Magenta: high red and blue, low green.
                        useCarPalette = (baseColor.r > 128 && baseColor.b > 128 && baseColor.g < 64);
                    } else {
                        // Other car bitmaps: every colour in range 1-9 or 33-41.
                        useCarPalette = true;
                    }
                }

                const Palette& selectedPalette = useCarPalette ? carPalette : palette;

                if (value >= selectedPalette.size()) {
                    continue;
                }

                int pixel_index = (y * entry.width) + (x + i);
                chunkyData[pixel_index] = selectedPalette[value];
            }
        }
    }

    return chunkyData;
}

std::vector<uint8_t> BitmapParser::ChunkyToPlanar(const std::vector<SrLibColor>& pixels,
    BitmapEntry_t entry, const Palette& palette, std::string* error) {
    if (entry.width <= 0 || entry.height <= 0 || entry.depth <= 0 || entry.width % 8 != 0) {
        if (error) *error = "Invalid bitmap dimensions or depth.";
        return {};
    }

    const size_t expectedPixels = (size_t)entry.width * (size_t)entry.height;
    if (pixels.size() != expectedPixels) {
        if (error) *error = "PNG pixel count does not match bitmap dimensions.";
        return {};
    }

    const int maxColors = 1 << entry.depth;
    if ((int)palette.size() < maxColors) {
        if (error) *error = "Selected palette does not contain enough colors for bitmap depth.";
        return {};
    }

    const int planeSize = (entry.width / 8) * entry.height;
    std::vector<uint8_t> planar((size_t)planeSize * entry.depth, 0);

    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; ++x) {
            const SrLibColor& pixel = pixels[(size_t)y * entry.width + x];
            int paletteIndex = -1;

            if (pixel.a < 128) {
                paletteIndex = 0;
            } else {
                for (int i = 0; i < maxColors; ++i) {
                    if (SameColor(pixel, palette[i])) {
                        paletteIndex = i;
                        break;
                    }
                }
            }

            if (paletteIndex < 0) {
                if (error) {
                    std::ostringstream oss;
                    oss << "Pixel at " << x << "," << y << " is not in the selected palette.";
                    *error = oss.str();
                }
                return {};
            }

            const int byteOffset = (y * entry.width + x) / 8;
            const uint8_t bit = 1 << (7 - (x % 8));
            for (int plane = 0; plane < entry.depth; ++plane) {
                if (paletteIndex & (1 << plane)) {
                    planar[(size_t)plane * planeSize + byteOffset] |= bit;
                }
            }
        }
    }

    return planar;
}

Palette BitmapParser::PreparePalette(const std::vector<uint16_t>& palette, bool ehb) {
    Palette rval;

    for (int i = 0; i < int(palette.size()); i++) {
        rval.push_back(GetRGBFromRGB4(palette[i]));
    }

    if (ehb) {
        for (int i = 0; i < int(palette.size()); i++) {
            SrLibColor color = rval[i];
            rval.push_back({
                static_cast<uint8_t>((color.r / 2) & 0xF0),
                static_cast<uint8_t>((color.g / 2) & 0xF0),
                static_cast<uint8_t>((color.b / 2) & 0xF0),
                255
            });
        }
    }

    return rval;
}

// Black except colours 1-10 (car shades) and their EHB counterparts.
Palette BitmapParser::PrepareCarPalette(const std::vector<uint16_t>& palette) {
    Palette rval;

    for (int i = 0; i < 32; i++) {
        rval.push_back({0, 0, 0, 255});
    }

    for (int i = 0; i < 9; i++) {
        rval.at(i + 1) = GetRGBFromRGB4(palette[i]);
    }

    for (int i = 0; i < int(palette.size()); i++) {
        SrLibColor color = rval[i];
        rval.push_back({
            static_cast<uint8_t>((color.r / 2) & 0xF0),
            static_cast<uint8_t>((color.g / 2) & 0xF0),
            static_cast<uint8_t>((color.b / 2) & 0xF0),
            255
        });
    }

    return rval;
}

void BitmapParser::SavePaletteToPNG(const Palette& palette, const std::string& filename) {
    const int BLOCK_SIZE = 16;
    const int numColors = palette.size();
    const int imageWidth = BLOCK_SIZE * numColors;
    const int imageHeight = BLOCK_SIZE;

    std::vector<unsigned char> image(imageWidth * imageHeight * 4);

    for (int y = 0; y < imageHeight; y++) {
        for (int x = 0; x < imageWidth; x++) {
            const SrLibColor& c = palette[x / BLOCK_SIZE];
            size_t pos = 4 * (y * imageWidth + x);
            image[pos + 0] = c.r;
            image[pos + 1] = c.g;
            image[pos + 2] = c.b;
            image[pos + 3] = c.a;
        }
    }

    unsigned char* png;
    size_t pngsize;
    unsigned error = lodepng_encode32(&png, &pngsize, image.data(), imageWidth, imageHeight);

    if (error) {
        std::cerr << "Failed to encode PNG: " << lodepng_error_text(error) << std::endl;
    } else {
        error = lodepng_save_file(png, pngsize, filename.c_str());
        if (error) {
            std::cerr << "Failed to save PNG file: " << lodepng_error_text(error) << std::endl;
        }
    }

    free(png);
}

SrLibColor BitmapParser::GetRGBFromRGB4(uint16_t rgb4) {
    SrLibColor color;
    color.r = ((rgb4 & 0xF00) >> 8) << 4;
    color.g = ((rgb4 & 0x0F0) >> 4) << 4;
    color.b = (rgb4 & 0x00F) << 4;
    color.a = 255;  // Fully opaque
    return color;
}

std::vector<std::pair<BitmapEntry_t, std::vector<uint8_t>>> BitmapParser::ReadAllImages(const std::string& filename) {
    std::vector<std::pair<BitmapEntry_t, std::vector<uint8_t>>> result;
    std::pair<int, std::vector<BitmapEntry_t>> entries = ReadEntries(filename);

    return result;
}

std::vector<uint8_t> BitmapParser::ReadImage(const std::vector<uint8_t>& data, BitmapEntry_t entry, int, int totalEntriesCount) {
    std::vector<uint8_t> packedData = GetPackedData(data, entry, totalEntriesCount);
    return UnpackData(RLE, entry, packedData);
}

std::pair<int, std::vector<DOSBitmapEntry_t>> BitmapParser::ReadEntriesDOS(const std::vector<uint8_t>& data) {
    std::vector<DOSBitmapEntry_t> result;
    VectorReader reader(data);

    uint16_t totalEntriesCount = reader.ReadUint16LE();

    for (int i = 0; i < totalEntriesCount; ++i) {
        DOSBitmapEntry_t newEntry = {};
        newEntry.width = reader.ReadUint16LE();
        newEntry.height = reader.ReadUint16LE();
        newEntry.sizeDecoded = reader.ReadUint16LE();
        newEntry.type = reader.ReadUint8();
        auto dictData = reader.ReadBytes(sizeof(newEntry.rleDictionary));
        memcpy(newEntry.rleDictionary, dictData.data(), sizeof(newEntry.rleDictionary));
        newEntry.padding = reader.ReadUint8();
        newEntry.offset = reader.ReadUint32LE();
        newEntry.sizeEncoded = reader.ReadUint16LE();

        result.push_back(newEntry);
    }

    return std::pair<int, std::vector<DOSBitmapEntry_t>>(totalEntriesCount, result);
}

std::vector<uint8_t> BitmapParser::GetPackedDataDOS(const std::vector<uint8_t>& data, const DOSBitmapEntry_t& entry, int totalEntriesCount) {
    size_t headerSize = 2 + (sizeof(DOSBitmapEntry_t) * (size_t)totalEntriesCount);
    size_t fileOffset = headerSize + (size_t)entry.offset;

    if (fileOffset > data.size() || (size_t)entry.sizeEncoded > data.size() - fileOffset) {
        throw std::runtime_error("Can't read DOS data because offset is larger than data size.");
    }

    return std::vector<uint8_t>(
        data.begin() + fileOffset,
        data.begin() + fileOffset + entry.sizeEncoded
    );
}

std::vector<uint8_t> BitmapParser::UnpackDataDOS(const DOSBitmapEntry_t& sprite, const std::vector<uint8_t>& data) {
    const std::vector<std::pair<int, uint8_t>> rle = RLE_DOS;

    int p = 0;
    int width = sprite.width / 8;
    int height = sprite.height;
    int depth = (sprite.type == 2) ? 1 : 4;
    size_t size_expected = (sprite.width * sprite.height * depth) / 8;

    std::vector<uint8_t> flat_bp;

    // Verify and skip 0xBC magic byte
    if (data.empty() || data[p] != 0xBC) {
        return flat_bp;
    }
    p++;

    int pos = 0;

    while (pos < width * height * depth) {
        if (p >= (int)data.size()) break;

        uint8_t b = data[p];

        bool found = false;
        const uint8_t* directory = sprite.rleDictionary;

        for (int i = 0; i < 16; i++) {
            if (directory[i] == 0) {
                break;
            }
            if (b == directory[i]) {
                for (int c = 0; c < rle[i].first; c++) {
                    pos++;
                    flat_bp.push_back(rle[i].second);
                }
                found = true;
                break;
            }
        }

        if (found) {
            p++;
            continue;
        }

        if (b == 0 || b == 0xFF) {
            uint8_t c = data[p + 1] & 0xFF;
            for (int i = 0; i < c + 1; i++) {
                flat_bp.push_back(b);
                pos++;
            }
            p += 2;
        } else {
            flat_bp.push_back(b);
            pos++;
            p++;
        }
    }

    if (flat_bp.size() < size_expected) {
        int missing = size_expected - flat_bp.size();
        flat_bp.insert(flat_bp.end(), missing, 0x0);
    }

    return flat_bp;
}

std::vector<uint8_t> BitmapParser::ReadImageDOS(const std::vector<uint8_t>& data, const DOSBitmapEntry_t& entry, int, int totalEntriesCount) {
    std::vector<uint8_t> packedData = GetPackedDataDOS(data, entry, totalEntriesCount);
    return UnpackDataDOS(entry, packedData);
}

Palette BitmapParser::GetDOSCarPalette(int spriteType, const std::vector<uint16_t>& carPalette) {
    Palette palette = GetDOSPalette(spriteType);
    if (carPalette.size() < DOS_CAR_RAMP_LIGHT + 1) {
        return palette;
    }

    // Entries 6 and 7 are the paint on every DOS car sprite, the same two registers the Amiga uses. The ramp's
    // steps 2 and 6 match the DOS blues for Navy.
    palette[DOS_CAR_SHADE_DARK] = GetRGBFromRGB4(carPalette[DOS_CAR_RAMP_DARK]);
    palette[DOS_CAR_SHADE_LIGHT] = GetRGBFromRGB4(carPalette[DOS_CAR_RAMP_LIGHT]);
    return palette;
}

Palette BitmapParser::GetDOSPalette(int spriteType) {
    Palette palette;
    for (int i = 0; i < 16; i++) {
        palette.push_back(DOS_PALETTE[i]);
    }

    if (spriteType == 1) {
        palette[0] = {0xFF, 0x00, 0x00, 0xFF};  // Red (transparency marker)
    } else if (spriteType == 2) {
        palette[0] = {0xFF, 0xFF, 0xFF, 0xFF};  // White (mask)
    }

    return palette;
}

std::vector<SrLibColor> BitmapParser::PlanarToChunky(const std::vector<uint8_t>& data,
    DOSBitmapEntry_t entry, const Palette& palette) {
    int depth = (entry.type == 2) ? 1 : 4;
    std::vector<SrLibColor> chunkyData(entry.width * entry.height);

    int planeSize = (entry.width / 8) * entry.height;

    for (int y = 0; y < entry.height; ++y) {
        for (int x = 0; x < entry.width; x += 8) {
            int offset = (y * entry.width + x) / 8;
            for (int i = 0; i < 8; ++i) {
                uint8_t value = 0;
                uint8_t shift = (1 << (7 - i));

                for (int p = 0; p < depth; ++p) {
                    if (shift & data[p * planeSize + offset]) {
                        value |= 1 << p;
                    }
                }

                if (value >= palette.size()) {
                    continue;
                }

                int pixel_index = (y * entry.width) + (x + i);
                chunkyData[pixel_index] = palette[value];
            }
        }
    }

    return chunkyData;
}

namespace {

// Big-endian readers over the executable image, bounds-checked against it.
struct Sr2ExeReader {
    const std::vector<uint8_t>& d;
    bool ok = true;

    int U16(size_t at) {
        if (at + 2 > d.size()) { ok = false; return 0; }
        return (int(d[at]) << 8) | int(d[at + 1]);
    }
    uint32_t U32(size_t at) {
        if (at + 4 > d.size()) { ok = false; return 0; }
        return (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) |
               (uint32_t(d[at + 2]) << 8) | uint32_t(d[at + 3]);
    }
    // A run of u16 out of a table at a runtime address.
    void Row(uint32_t addr, int* out, int count) {
        const size_t at = Sr2ExeOffset(addr);
        for (int i = 0; i < count; i++) out[i] = U16(at + size_t(i) * 2);
    }
    std::string Text(size_t at) {
        if (at >= d.size()) { ok = false; return {}; }
        size_t end = at;
        while (end < d.size() && d[end]) ++end;
        return std::string(reinterpret_cast<const char*>(d.data() + at), end - at);
    }
};

} // namespace

std::vector<Sr2PartEntry> SrLibReadSr2Parts(const std::vector<uint8_t>& sr2ExeData) {
    Sr2ExeReader r{sr2ExeData};
    const size_t base = Sr2ExeOffset(SR2_PARTS_CATALOG_ADDR);

    std::vector<Sr2PartEntry> parts;
    parts.reserve(SR2_PARTS_CATALOG_COUNT);
    for (int i = 0; i < SR2_PARTS_CATALOG_COUNT; i++) {
        const size_t rec = base + size_t(i) * 16;
        Sr2PartEntry e;
        e.price      = r.U16(rec + 0);
        e.configBits = r.U16(rec + 2);
        e.typeId     = r.U16(rec + 4);
        e.fitMask    = r.U16(rec + 6);
        e.group      = r.U16(rec + 8);
        // The ad pointer is stored relative to the string hunk; the Amiga
        // loader relocates it, and reading the raw file means adding the base.
        e.ad         = r.Text(SR2_EXE_PART_TEXT_BASE + r.U32(rec + 10));
        e.adLayout   = sr2ExeData.size() > rec + 14 ? int(sr2ExeData[rec + 14]) : 0;
        if (!r.ok) return {};
        // Every record names a slot and costs something.
        if (e.group < 0 || e.group >= Sr2PartGroupCount || e.price <= 0) return {};
        parts.push_back(std::move(e));
    }
    return parts;
}

Sr2MechTables SrLibReadSr2MechTables(const std::vector<uint8_t>& sr2ExeData) {
    Sr2ExeReader r{sr2ExeData};
    Sr2MechTables t;

    for (int i = 0; i < 3; i++) r.Row(0x263AF8 + uint32_t(i) * 10, t.rpmGain[i], 5);
    for (int i = 0; i < 4; i++) r.Row(0x263AD0 + uint32_t(i) * 10, t.gearRatio[i], 5);
    for (int i = 0; i < 3; i++) r.Row(0x263B16 + uint32_t(i) * 4, t.powerBand[i], 2);
    for (int i = 0; i < 3; i++) r.Row(0x263B22 + uint32_t(i) * 18, t.manifoldScore[i], 9);
    r.Row(0x263B66, t.redline, 3);
    r.Row(0x263B6C, t.gearCount, 4);
    r.Row(0x263B74, t.tireGrip, 3);
    r.Row(0x263B7A, t.tierScore, 9);
    r.Row(0x263B58, t.engineScore, 3);
    r.Row(0x263B5E, t.transScore, 4);
    if (!r.ok) return t;

    // Cheap shape checks, so a file that does not hold these tables gives nothing rather than a car with a
    // 3-rpm redline.
    for (int i = 0; i < 3; i++) {
        if (t.powerBand[i][0] <= 0 || t.powerBand[i][1] <= t.powerBand[i][0]) return t;
        if (t.redline[i] != t.powerBand[i][1]) return t;
    }
    for (int i = 1; i < 9; i++) {
        if (t.tierScore[i] <= t.tierScore[i - 1]) return t;
    }
    for (int i = 0; i < 4; i++) {
        if (t.gearCount[i] < 3 || t.gearCount[i] > 5) return t;
    }
    t.valid = true;
    return t;
}

std::vector<std::vector<std::string>> SrLibReadSr2Messages(
        const std::vector<uint8_t>& sr2ExeData) {
    std::vector<std::vector<std::string>> out;
    const size_t needed = SR2_MESSAGE_TABLE_OFFSET +
                          static_cast<size_t>(SR2_MESSAGE_COUNT) * SR2_MESSAGE_RECORD_SIZE;
    if (sr2ExeData.size() < needed) {
        return out;
    }
    auto beU16 = [&](size_t at) -> uint32_t {
        return (static_cast<uint32_t>(sr2ExeData[at]) << 8) | sr2ExeData[at + 1];
    };
    auto beU32 = [&](size_t at) -> uint32_t {
        return (static_cast<uint32_t>(sr2ExeData[at]) << 24) |
               (static_cast<uint32_t>(sr2ExeData[at + 1]) << 16) |
               (static_cast<uint32_t>(sr2ExeData[at + 2]) << 8) |
                static_cast<uint32_t>(sr2ExeData[at + 3]);
    };
    out.resize(SR2_MESSAGE_COUNT);
    for (int i = 0; i < SR2_MESSAGE_COUNT; ++i) {
        const size_t rec = SR2_MESSAGE_TABLE_OFFSET +
                           static_cast<size_t>(i) * SR2_MESSAGE_RECORD_SIZE;
        // The style decides how many of the two pointers are drawn: style 2 is
        // the only two line box, everything else is one line.
        const int lines =
            beU16(rec) == SR2_MESSAGE_TWO_LINE_STYLE ? SR2_MESSAGE_MAX_LINES : 1;
        for (int l = 0; l < lines; ++l) {
            const uint32_t ptr = beU32(rec + 2 + static_cast<size_t>(l) * 4);
            // An unused record holds a null pointer rather than a string.
            if (ptr == 0) {
                break;
            }
            const size_t at = SR2_DATA67_FILE_OFFSET + ptr;
            if (at >= sr2ExeData.size()) {
                break;
            }
            size_t end = at;
            while (end < sr2ExeData.size() && sr2ExeData[end] != 0) {
                ++end;
            }
            out[static_cast<size_t>(i)].emplace_back(
                reinterpret_cast<const char*>(sr2ExeData.data() + at), end - at);
        }
    }
    return out;
}

std::vector<std::vector<uint16_t>> SrLibReadSr2Palettes(const std::vector<uint8_t>& sr2ExeData) {
    const size_t need = SR2_EXE_PALETTE_OFFSET +
                        size_t(SR2_PALETTE_COUNT) * SR2_PALETTE_SIZE * 2;
    if (sr2ExeData.size() < need) return {};

    std::vector<std::vector<uint16_t>> palettes;
    const uint8_t* src = sr2ExeData.data() + SR2_EXE_PALETTE_OFFSET;
    for (int p = 0; p < SR2_PALETTE_COUNT; p++) {
        std::vector<uint16_t> pal(SR2_PALETTE_SIZE);
        for (int c = 0; c < SR2_PALETTE_SIZE; c++, src += 2) {
            pal[c] = (uint16_t(src[0]) << 8) | src[1];
            // An Amiga 12-bit colour never sets the top nibble. Anything else
            // means this is not the palette table, so take none of it.
            if (pal[c] > 0x0FFF) return {};
        }
        palettes.push_back(std::move(pal));
    }
    return palettes;
}

LibAssetType BitmapParser::GetAssetType(SrLibFile location, int index, SrGame game) {
    // Street Rod 2's other spritesheets and masks are not indexed yet.
    if (game == SrGame::SR2) {
        if (index == SR2_GAS_STATION_PUMP_MASK) return LibAssetMask;
        return ((index >= SR2_CAR_TIRES_BRAND && index <= SR2_CAR_TIRES_STOCK) ||
                index == SR2_GAS_STATION_ATTENDANT_ANIMATION) ? LibAssetSpritesheet : LibAssetRegular;
    }
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                case PHONEBOOK_GUY_1:
                case PHONEBOOK_GUY:
                case DECALS:
                case GUS_GAS_ATTENDANT:
                case DINER_WAITRESS_ANIM_LEFT:
                case DINER_WAITRESS_ANIM_RIGHT:
                    return LibAssetSpritesheet;
                case GUS_GAS_MASK:
                case DINER_SCREEN_MASK:
                    return LibAssetMask;
                default:
                    return LibAssetRegular;
            }
        }

        default:
            return LibAssetRegular;
    }
}

bool BitmapParser::LoadsOpaque(SrLibFile location, int index, SrGame game) {
    if (game == SrGame::SR2) return location == SrLibFileAmigaLibP && index >= 71 && index <= 73;   // shifter plates
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset asset = static_cast<AmigaLibAsset>(index);
            switch (asset) {
                case GUS_GAS_MASK:
                case DINER_SCREEN_MASK:
                // The road surface runs under the dashboard's top rows.
                case RACING_INTERIOR_1:
                // Each tuning frame has to cover the one underneath.
                case ENGINE_BELT_ANIMATION:
                case ENGINE_PISTON_LEFT_ANIMATION:
                case ENGINE_PISTON_RIGHT_ANIMATION:
                case ENGINE_CRANK_ANIMATION:
                case ENGINE_FLYWHEEL_ANIMATION:
                case ENGINE_CAM_ANIMATION:
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

std::pair<int, int> BitmapParser::GetSpritesheetInfo(SrLibFile location, int index, SrGame game) {
    if (game == SrGame::SR2) {
        return (index >= SR2_CAR_TIRES_BRAND && index <= SR2_CAR_TIRES_STOCK) ? std::make_pair(1, 4)
                                                                              : std::make_pair(0, 0);
    }
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                    return std::make_pair(1, 4);
                case PHONEBOOK_GUY_1:
                case PHONEBOOK_GUY:
                    return std::make_pair(2, 5);
                case DECALS:
                    return std::make_pair(2, 4);
                case GUS_GAS_ATTENDANT:
                    return std::make_pair(1, 4);
                default:
                    return std::make_pair(0, 0);
            }
        }

        default:
            return std::make_pair(0, 0);
    }
}

std::vector<uint16_t> BitmapParser::GetStockPalette(StockPalette palette) {
    switch (palette) {
        case StockPaletteAmigaDiner:
            return PAL1;
        case StockPaletteAmigaGasStation:
            return PAL_GAS;
        case StockPaletteAmigaDriving:
            return PAL_DRIVING;
        case StockPaletteAmigaDrivingLow:
            return PAL_DRIVING_LOW;
        default:
            return PAL1;
    }
}

std::vector<uint16_t> BitmapParser::GetPalette(SrLibFile location, int index, SrGame game,
                                               const std::vector<std::vector<uint16_t>>& gamePalettes) {
    if (game == SrGame::SR2) {
        // Street Rod 2 has nine palettes; which screen picks which is not traced in general.
        // The driving copperlist reloads pens 6 to 9, 11 and 12 with a skin ramp from scanline 166 (game y 122)
        // down, which is where the hands are drawn; the rest of the palette is the driving one.
        const bool hands = (index >= SR2_HAND_GRIP_LEFT && index <= SR2_ARM_RIGHT) ||
                           index == SR2_RACING_RIGHT_HAND_SHIFTING;
        if (hands && gamePalettes.size() > 1) {
            static constexpr struct { size_t pen; uint16_t color; } kSkin[] = {
                {6, 0x0F96}, {7, 0x0D94}, {8, 0x0D62}, {9, 0x0A50}, {11, 0x0841}, {12, 0x0730}};
            std::vector<uint16_t> skin = gamePalettes[1];
            for (const auto& s : kSkin) skin[s.pen] = s.color;
            return skin;
        }
        const bool driving = (index >= 36 && index <= 98) || index >= 274;
        // The drive-in's palette is the main one with pens 30 and 31 pink instead of blue: the neon script.
        const bool driveIn = index == SR2_DRIVE_IN_SCREEN ||
                             (index >= SR2_DRIVE_IN_WAITRESS_MASK && index <= SR2_DINER_WAITRESS_ANIM_RIGHT);
        const size_t pal = driving ? 1 : driveIn ? 3 : 0;
        if (pal < gamePalettes.size()) return gamePalettes[pal];
        return driving ? std::vector<uint16_t>(PAL_DRIVING)
                       : std::vector<uint16_t>(PAL1);
    }
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;

            switch (a) {
                case GUS_GAS_SCREEN:
                case GUS_GAS_ATTENDANT:
                case GUS_GAS_PUMP1:
                case GUS_GAS_PUMP2:
                case GUS_GAS_PUMP3_MASK:
                case GUS_GAS_PUMP_FILLING:
                    return PAL_GAS;
                default:
                    break; // Continue to general logic below
            }

            // Their colours are lib indices 10 and 22: white body and blue frame in PAL1, grey in PAL_DRIVING_LOW.
            if (a == SPEECH_BUBBLE_ARROW_RIGHT || a == SPEECH_BUBBLE_ARROW_LEFT ||
                a == SPEECH_BUBBLE_ARROW_LEFT_TWO_ROWS) {
                return PAL1;
            }

            // PAL_DRIVING_LOW gives the signs wrong colours.
            if (a >= RACING_PALM_TREE && a <= RACING_SIGN_PLAY_BLOCKOUT) {
                return PAL_DRIVING;
            }

            // Above the copperlist split: colours from the player car's ramp.
            if (a == RACING_HOOD || a == RACING_INTERIOR_1 || a == RACING_CRASHED_INTERIOR) {
                return PAL_DRIVING_HOOD;
            }

            // A menu screen, though its indices fall in the driving-backdrop range.
            if (a >= NEWSPAPER_MENU && a <= NEWSPAPER_NO_SOLDIERS_IN_VIETNAM) {
                return PAL1;
            }

            if (index < 45) {
                return PAL_DRIVING_LOW;
            } else if (index >= 45 && index <= 80) {
                return PAL_DRIVING_LOW;
            } else if (index <= 176) {
                return PAL_DRIVING;
            } else {
                return PAL1;
            }
        }

        default:
            return PAL1;
    }
}

std::pair<int, std::vector<BitmapEntry_t>> BitmapParser::ReadEntries(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::make_pair(0, std::vector<BitmapEntry_t>());
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
       std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::make_pair(0, std::vector<BitmapEntry_t>());
    }

    return ReadEntries(buffer);
}

std::vector<uint8_t> BitmapParser::GetPackedData(const std::string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::vector<uint8_t>();
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::vector<uint8_t>();
    }

    return GetPackedData(buffer, bitmapEntry, totalEntriesCount);
}

std::vector<uint8_t> BitmapParser::ReadImage(const std::string& filename, BitmapEntry_t entry, int index, int totalEntriesCount) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::vector<uint8_t>();
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::vector<uint8_t>();
    }

    return ReadImage(buffer, entry, index, totalEntriesCount);
}



void SrLibDataCleanupCarString(char* str) {
    // A double backtick becomes a space.
    char* src = str;
    char* dst = str;
    while (*src) {
        if (src[0] == '`' && src[1] == '`') {
            *dst++ = ' ';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    // A leading '+' marks a feature line.
    if (str[0] == '+') {
        memmove(str, str + 1, strlen(str));  // includes null terminator
    }
}

std::vector<std::string> SrLibReadStrings(const uint8_t* data, int offset, size_t dataSize) {
    std::vector<std::string> result;
    const char* ptr = (const char*)data + offset;
    while (ptr < (const char*)data + dataSize) {
        result.push_back(std::string(ptr));
        ptr += strlen(ptr) + 1;
    }
    return result;
}

static bool ParseClassifiedEntry(const uint8_t* data, size_t dataSize, const uint8_t*& ptr, ClassifiedEntry_t& outClassified, int maxLines = 0) {
    if (ptr >= data + dataSize) {
        return false;
    }

    const char* nameStr = (const char*)ptr;
    size_t nameLen = strlen(nameStr);
    if (ptr + nameLen + 1 > data + dataSize) {
        return false;
    }
    ptr += nameLen + 1;  // Skip past null terminator

    char yearStr[5] = {0};
    strncpy(yearStr, nameStr, 4);
    outClassified.year = atoi(yearStr);

    char* name = (char*)malloc(nameLen + 1);
    if (!name) return false;
    strcpy(name, nameStr);
    SrLibDataCleanupCarString(name);
    outClassified.carName = name;

    static constexpr size_t kAdTextCapacity = 2048;
    char* adText = (char*)malloc(kAdTextCapacity);  // Larger buffer for full ad text
    if (!adText) {
        free(name);
        return false;
    }
    adText[0] = '\0';
    size_t adTextLen = 0;

    // Street Rod ends an ad on an empty string.
    for (int line_i = 0; maxLines <= 0 || line_i < maxLines; ++line_i) {
        if (ptr >= data + dataSize) {
            free(name);
            free(adText);
            return false;
        }

        const char* lineStr = (const char*)ptr;
        size_t lineLen = strlen(lineStr);
        if (ptr + lineLen + 1 > data + dataSize) {
            free(name);
            free(adText);
            return false;
        }
        ptr += lineLen + 1;  // Skip past null terminator

        if (!lineStr[0]) {
            break;  // empty string = end of entry
        }

        // A '+' line goes in the right column on the same row as the car name.
        bool isFeature = (lineStr[0] == '+');

        char* line = (char*)malloc(lineLen + 1);
        if (!line) {
            free(name);
            free(adText);
            return false;
        }
        strcpy(line, lineStr);
        SrLibDataCleanupCarString(line);  // strips leading '+' if isFeature

        const char* dollarPos = strchr(line, '$');
        // Before `line` is freed below.
        bool hasPrice = dollarPos != nullptr;
        int parsedPrice = hasPrice ? atoi(dollarPos + 1) : 0;

        // Lines separated by '\n', feature lines prefixed with '\t'. Bounded: an oversized ad stops growing.
        size_t appendLen = (adText[0] ? 1 : 0) + (isFeature ? 1 : 0) + strlen(line);
        if (adTextLen + appendLen < kAdTextCapacity) {
            if (adText[0]) { strcat(adText, "\n"); adTextLen += 1; }
            if (isFeature) { strcat(adText, "\t"); adTextLen += 1; }
            strcat(adText, line);  // price lines keep their '$' in the string
            adTextLen += strlen(line);
        }

        free(line);

        if (hasPrice) {
            outClassified.price = parsedPrice;
            break;
        }
    }
    outClassified.adText = adText;
    return true;
}

// SR_DATA_OFFSETS_START lives in libp.h.
static constexpr size_t SR_DATA_CLICKBOX_START = 0x484;
static constexpr size_t SR_DATA_CLICKBOX_GASSTATION_START = 0x68c;
static constexpr size_t SR_DATA_CLASSIFIEDS_START = 0x854;

static size_t SrBoundedStrnlen(const char* str, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && str[len] != '\0') {
        len++;
    }
    return len;
}

static bool ValidateByteValue(int value, const char* fieldName, int carIndex, std::string* error) {
    if (value < 0 || value > 255) {
        if (error) {
            std::ostringstream oss;
            oss << fieldName << " for car " << carIndex << " must be between 0 and 255.";
            *error = oss.str();
        }
        return false;
    }
    return true;
}

static bool ValidateSrDataBounds(size_t dataSize, std::string* error) {
    const size_t offsetsEnd = SR_DATA_OFFSETS_START + CAR_PLAYER_MODEL_COUNT * 12;
    const size_t clickEnd = SR_DATA_CLICKBOX_START + CAR_PLAYER_MODEL_COUNT * 20;
    const size_t gasClickEnd = SR_DATA_CLICKBOX_GASSTATION_START + CAR_PLAYER_MODEL_COUNT * 4;
    if (dataSize < offsetsEnd || dataSize < clickEnd || dataSize < gasClickEnd || dataSize < SR_DATA_CLASSIFIEDS_START) {
        if (error) *error = "SR_data is too small for known car metadata blocks.";
        return false;
    }
    return true;
}

static bool FindClassifiedsEnd(const std::vector<uint8_t>& data, size_t* outEnd, std::string* error) {
    if (data.size() < SR_DATA_CLASSIFIEDS_START) {
        if (error) *error = "SR_data is too small for classifieds block.";
        return false;
    }

    const uint8_t* ptr = data.data() + SR_DATA_CLASSIFIEDS_START;
    const uint8_t* end = data.data() + data.size();
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        if (ptr >= end) {
            if (error) *error = "Classifieds block ended before all car entries.";
            return false;
        }

        const char* nameStr = (const char*)ptr;
        size_t nameLen = SrBoundedStrnlen(nameStr, (size_t)(end - ptr));
        if (ptr + nameLen >= end) {
            if (error) *error = "Classified car name is missing a terminator.";
            return false;
        }
        ptr += nameLen + 1;

        while (true) {
            if (ptr >= end) {
                if (error) *error = "Classified ad text is missing a terminator.";
                return false;
            }
            const char* lineStr = (const char*)ptr;
            size_t lineLen = SrBoundedStrnlen(lineStr, (size_t)(end - ptr));
            if (ptr + lineLen >= end) {
                if (error) *error = "Classified ad line is missing a terminator.";
                return false;
            }
            ptr += lineLen + 1;
            if (lineLen == 0 || strchr(lineStr, '$')) {
                break;
            }
        }
    }

    *outEnd = (size_t)(ptr - data.data());
    return true;
}

static void AppendCString(std::vector<uint8_t>& out, const std::string& value) {
    out.insert(out.end(), value.begin(), value.end());
    out.push_back(0);
}

static void AppendClassified(std::vector<uint8_t>& out, const ClassifiedEntry_t& classified) {
    AppendCString(out, classified.carName ? classified.carName : "");

    std::string adText = classified.adText ? classified.adText : "";
    size_t start = 0;
    while (start <= adText.size()) {
        size_t nl = adText.find('\n', start);
        std::string line = adText.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty() && line[0] == '\t') {
            line[0] = '+';
        }
        AppendCString(out, line);
        if (line.find('$') != std::string::npos || nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
}

CarMetadata_t* DataParser::SrLibDataReadDataFromBuffer(const uint8_t* data, size_t dataSize, int* outCount) {
    const int OFFSETS_START = 0x34C;
    const int CLICKBOX_START = 0x484;
    const int CLICKBOX_GASSTATION_START = 0x68c;
    const int CLASSIFIEDS_START = 0x854;

    if (!data || !outCount) return NULL;
    if (!ValidateSrDataBounds(dataSize, nullptr)) return NULL;

    *outCount = CAR_PLAYER_MODEL_COUNT;
    CarMetadata_t* result = (CarMetadata_t*)calloc(CAR_PLAYER_MODEL_COUNT, sizeof(CarMetadata_t));
    if (!result) return NULL;

    const uint8_t* ptr = data + OFFSETS_START;

    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        BitmapOffset_t newEntry = {};
        newEntry.offsetXRearTire = *ptr++;
        newEntry.offsetXFrontTire = *ptr++;
        newEntry.offsetXDriver = *ptr++;
        newEntry.offsetYDriver = *ptr++;
        newEntry.offsetXChoppedRoof = *ptr++;
        newEntry.offsetXAirScoop = *ptr++;
        newEntry.offsetYAirScoop = *ptr++;
        newEntry.offsetYRearBumper = *ptr++;
        newEntry.offsetXFrontBumper = *ptr++;
        newEntry.offsetYFrontBumper = *ptr++;
        newEntry.offsetXDecal = *ptr++;
        newEntry.offsetYDecal = *ptr++;

        // offsetYTires is filled in by the caller. The gas cap offset comes from the click box point below, so the
        // sprite and zone agree.

        result[i].offsets = newEntry;
    }

    // One of CAR_SCOOP1..4 per body; one-based, 0 = none.
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        const uint8_t* rec = data + SR_DATA_CAR_SPRITES_START + i * 10 + 4;
        const uint16_t raw = (uint16_t(rec[0]) << 8) | rec[1];
        result[i].airScoop = raw ? int(raw) - 1 : -1;
    }

    ptr = data + CLASSIFIEDS_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        if (!ParseClassifiedEntry(data, dataSize, ptr, result[i].classified)) {
            goto cleanup;
        }
    }

    ptr = data + CLICKBOX_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarClickBoxes_t newClickBoxes;
        ClickBox_t rearBumper, choppedRoof, transmission, engine, frontBumper;

        rearBumper.x = *ptr++;
        rearBumper.y = *ptr++;
        rearBumper.width = *ptr++;
        rearBumper.height = *ptr++;
        newClickBoxes.clickBoxRearBumper = rearBumper;

        choppedRoof.x = *ptr++;
        choppedRoof.y = *ptr++;
        choppedRoof.width = *ptr++;
        choppedRoof.height = *ptr++;
        newClickBoxes.clickBoxChoppedRoof = choppedRoof;

        transmission.x = *ptr++;
        transmission.y = *ptr++;
        transmission.width = *ptr++;
        transmission.height = *ptr++;
        newClickBoxes.clickBoxTransmission = transmission;

        engine.x = *ptr++;
        engine.y = *ptr++;
        engine.width = *ptr++;
        engine.height = *ptr++;
        newClickBoxes.clickBoxEngine = engine;

        frontBumper.x = *ptr++;
        frontBumper.y = *ptr++;
        frontBumper.width = *ptr++;
        frontBumper.height = *ptr++;
        newClickBoxes.clickBoxFrontBumper = frontBumper;

        result[i].clickBoxes = newClickBoxes;
    }

    ptr = data + CLICKBOX_GASSTATION_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarMetadata_t& metadataEntry = result[i];
        CarClickBoxes_t& clickBoxes = metadataEntry.clickBoxes;

        clickBoxes.clickBoxGasCap.x = *ptr++;
        clickBoxes.clickBoxGasCap.y = *ptr++;
        clickBoxes.clickBoxGasCap.width = 12;
        clickBoxes.clickBoxGasCap.height = 12;
        metadataEntry.offsets.offsetXGasCap = clickBoxes.clickBoxGasCap.x;
        metadataEntry.offsets.offsetYGasCap = clickBoxes.clickBoxGasCap.y;

        clickBoxes.clickBoxRearWindow.x = *ptr++;
        clickBoxes.clickBoxRearWindow.y = *ptr++;
        clickBoxes.clickBoxRearWindow.width = 24;
        clickBoxes.clickBoxRearWindow.height = 24;

        clickBoxes.clickBoxFrontWindow.x = *ptr++;
        clickBoxes.clickBoxFrontWindow.y = *ptr++;
        clickBoxes.clickBoxFrontWindow.width = 24;
        clickBoxes.clickBoxFrontWindow.height = 24;
    }


    return result;

cleanup:
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        SrLibDataFreeClassified(&result[i].classified);
    }
    free(result);
    *outCount = 0;
    return NULL;
}

std::vector<CarSpriteRefs> DataParser::ParseCarSpritesAmiga(const std::vector<uint8_t>& sr_data,
                                                            SrGame game) {
    const bool isSr2 = game == SrGame::SR2;
    const size_t start = isSr2 ? SR2_DATA_CAR_SPRITES_START : SR_DATA_CAR_SPRITES_START;
    const int count = isSr2 ? SR2_DATA_CARSPEC_ENTRY_COUNT : SR_DATA_CARSPEC_ENTRY_COUNT;
    const size_t specStart = isSr2 ? SR2_DATA_DRIVER_ENTRY_COUNT * 10
                                   : SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    const size_t specSize = isSr2 ? SR2_DATA_CARSPEC_RECORD_SIZE : SR_DATA_CARSPEC_RECORD_SIZE;
    const size_t bodyAt = isSr2 ? 16 : 12;

    std::vector<CarSpriteRefs> out;
    if (sr_data.size() < start + size_t(count) * 10) return out;
    if (sr_data.size() < specStart + size_t(count) * specSize) return out;

    auto be16 = [&](size_t at) {
        return (int(sr_data[at]) << 8) | int(sr_data[at + 1]);
    };
    // Sprite numbers here are one-based, 0 meaning "no such variant".
    auto sprite = [&](size_t at) { const int v = be16(at); return v ? v - 1 : -1; };

    for (int i = 0; i < count; i++) {
        const size_t rec = start + size_t(i) * 10;
        CarSpriteRefs refs;
        refs.dashboard           = sr_data[rec + 1];
        refs.bodyStyle           = sr_data[rec];
        refs.body                = sprite(specStart + size_t(i) * specSize + bodyAt);
        refs.choppedRoof         = sprite(rec + 2);
        refs.airScoop            = sprite(rec + 4);
        refs.rearBumperStripped  = sprite(rec + 6);
        refs.frontBumperStripped = sprite(rec + 8);
        out.push_back(refs);
    }
    return out;
}

CarMetadata_t* DataParser::SrLibDataReadDataFromBufferSR2(const uint8_t* data, size_t dataSize, int* outCount) {
    if (!data || !outCount) return NULL;
    *outCount = 0;

    const int count = SR2_DATA_CARSPEC_ENTRY_COUNT;
    const size_t specBlock = SR2_DATA_DRIVER_ENTRY_COUNT * 10;
    // Everything up to the end of the string offset table has to be there.
    const size_t needed = SR2_DATA_STRING_TABLE_START + size_t(count) * 2;
    if (dataSize < needed || dataSize < size_t(SR2_DATA_TEXT_BLOB_OFFSET)) return NULL;

    CarMetadata_t* result = (CarMetadata_t*)calloc(count, sizeof(CarMetadata_t));
    if (!result) return NULL;

    const uint8_t* ptr = data + SR2_DATA_OFFSETS_START;
    for (int i = 0; i < count; i++) {
        BitmapOffset_t e = {};
        e.offsetXRearTire     = *ptr++;
        e.offsetXFrontTire    = *ptr++;
        e.offsetXDriver       = *ptr++;
        e.offsetYDriver       = *ptr++;
        e.offsetXChoppedRoof  = *ptr++;
        e.offsetXAirScoop     = *ptr++;
        e.offsetYAirScoop     = *ptr++;
        e.offsetYRearBumper   = *ptr++;
        e.offsetXFrontBumper  = *ptr++;
        e.offsetYFrontBumper  = *ptr++;
        e.offsetXDecal        = *ptr++;
        e.offsetYDecal        = *ptr++;
        // Street Rod 2 has no stickers; these two bytes are where the gas cap sits (the car draw routine reads them
        // into the cap position, FUN_0023760c), and the cap's click box is the same spot.
        e.offsetXGasCap = e.offsetXDecal;
        e.offsetYGasCap = e.offsetYDecal;
        result[i].clickBoxes.clickBoxGasCap = {e.offsetXGasCap, e.offsetYGasCap, 12, 12};
        // offsetYTires is the caller's, same as Street Rod's.
        result[i].offsets = e;
    }

    // Hood scoop: the sprite ref table names one of the four scoops, one-based.
    for (int i = 0; i < count; i++) {
        const uint8_t* rec = data + SR2_DATA_CAR_SPRITES_START + i * 10 + 4;
        const uint16_t raw = (uint16_t(rec[0]) << 8) | rec[1];
        result[i].airScoop = raw ? int(raw) - 1 : -1;
    }

    ptr = data + SR2_DATA_CLICKBOX_START;
    for (int i = 0; i < count; i++) {
        ClickBox_t* boxes[5] = {
            &result[i].clickBoxes.clickBoxRearBumper,
            &result[i].clickBoxes.clickBoxChoppedRoof,
            &result[i].clickBoxes.clickBoxTransmission,
            &result[i].clickBoxes.clickBoxEngine,
            &result[i].clickBoxes.clickBoxFrontBumper,
        };
        for (ClickBox_t* box : boxes) {
            box->x = *ptr++;
            box->y = *ptr++;
            box->width = *ptr++;
            box->height = *ptr++;
        }
    }

    // Ads are not laid out end to end the way Street Rod's are.
    for (int i = 0; i < count; i++) {
        const uint8_t* spec = data + specBlock + size_t(i) * SR2_DATA_CARSPEC_RECORD_SIZE;
        const uint16_t price = (uint16_t(spec[0]) << 8) | spec[1];
        if (price == 0) continue;

        const uint8_t* tab = data + SR2_DATA_STRING_TABLE_START + size_t(i) * 2;
        const size_t at = size_t(SR2_DATA_TEXT_BLOB_OFFSET) +
                          ((size_t(tab[0]) << 8) | tab[1]);
        if (at >= dataSize) continue;

        const uint8_t* adPtr = data + at;
        ParseClassifiedEntry(data, dataSize, adPtr, result[i].classified, spec[14]);
    }

    *outCount = count;
    return result;
}

std::vector<uint8_t> DataParser::SrLibDataWriteCarMetadataToBuffer(const std::vector<uint8_t>& data,
    const std::vector<CarMetadata_t>& metadata, std::string* error) {
    if (metadata.size() != CAR_PLAYER_MODEL_COUNT) {
        if (error) *error = "Expected 25 car metadata entries.";
        return {};
    }
    if (!ValidateSrDataBounds(data.size(), error)) {
        return {};
    }

    size_t classifiedsEnd = 0;
    if (!FindClassifiedsEnd(data, &classifiedsEnd, error)) {
        return {};
    }

    std::vector<uint8_t> out = data;

    uint8_t* ptr = out.data() + SR_DATA_OFFSETS_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        const BitmapOffset_t& offsets = metadata[i].offsets;
        *ptr++ = offsets.offsetXRearTire;
        *ptr++ = offsets.offsetXFrontTire;
        *ptr++ = offsets.offsetXDriver;
        *ptr++ = offsets.offsetYDriver;
        *ptr++ = offsets.offsetXChoppedRoof;
        *ptr++ = offsets.offsetXAirScoop;
        *ptr++ = offsets.offsetYAirScoop;
        *ptr++ = offsets.offsetYRearBumper;
        *ptr++ = offsets.offsetXFrontBumper;
        *ptr++ = offsets.offsetYFrontBumper;
        *ptr++ = offsets.offsetXDecal;
        *ptr++ = offsets.offsetYDecal;
    }

    ptr = out.data() + SR_DATA_CLICKBOX_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        const CarClickBoxes_t& clickBoxes = metadata[i].clickBoxes;
        const ClickBox_t boxes[] = {
            clickBoxes.clickBoxRearBumper,
            clickBoxes.clickBoxChoppedRoof,
            clickBoxes.clickBoxTransmission,
            clickBoxes.clickBoxEngine,
            clickBoxes.clickBoxFrontBumper,
        };
        for (const auto& box : boxes) {
            if (!ValidateByteValue(box.x, "click x", i, error) ||
                !ValidateByteValue(box.y, "click y", i, error) ||
                !ValidateByteValue(box.width, "click width", i, error) ||
                !ValidateByteValue(box.height, "click height", i, error)) {
                return {};
            }
            *ptr++ = (uint8_t)box.x;
            *ptr++ = (uint8_t)box.y;
            *ptr++ = (uint8_t)box.width;
            *ptr++ = (uint8_t)box.height;
        }
    }

    ptr = out.data() + SR_DATA_CLICKBOX_GASSTATION_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        const CarClickBoxes_t& clickBoxes = metadata[i].clickBoxes;
        if (!ValidateByteValue(clickBoxes.clickBoxGasCap.x, "gas cap click x", i, error) ||
            !ValidateByteValue(clickBoxes.clickBoxGasCap.y, "gas cap click y", i, error) ||
            !ValidateByteValue(clickBoxes.clickBoxRearWindow.x, "rear window click x", i, error) ||
            !ValidateByteValue(clickBoxes.clickBoxRearWindow.y, "rear window click y", i, error)) {
            return {};
        }
        *ptr++ = (uint8_t)clickBoxes.clickBoxGasCap.x;
        *ptr++ = (uint8_t)clickBoxes.clickBoxGasCap.y;
        *ptr++ = (uint8_t)clickBoxes.clickBoxRearWindow.x;
        *ptr++ = (uint8_t)clickBoxes.clickBoxRearWindow.y;
    }

    std::vector<uint8_t> rebuilt;
    rebuilt.reserve(out.size() + 256);
    rebuilt.insert(rebuilt.end(), out.begin(), out.begin() + SR_DATA_CLASSIFIEDS_START);
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; ++i) {
        AppendClassified(rebuilt, metadata[i].classified);
    }
    rebuilt.insert(rebuilt.end(), out.begin() + classifiedsEnd, out.end());

    return rebuilt;
}

PartMetadata_t* DataParser::SrLibDataReadPartsFromBuffer(const uint8_t* data, size_t dataSize, int* outCount,
                                                         SrGame game) {
    // From the executable, not SR_data. Street Rod: 6 engines, 13 transmissions, 3 carburetors, 15 manifolds, 3 tires.
    // Street Rod 2 adds exhausts, mufflers, differentials and more engines and manifolds, 64 ads.
    const bool sr2 = game == SrGame::SR2;
    const int CLASSIFIEDS_PARTS_START = sr2 ? 0xE194 : 0xA4CE;
    const int CLASSIFIEDS_PARTS_COUNT = sr2 ? 64 : 40;

    if (!data || !outCount || dataSize < CLASSIFIEDS_PARTS_START) return NULL;

    *outCount = CLASSIFIEDS_PARTS_COUNT;
    PartMetadata_t* result = (PartMetadata_t*)calloc(CLASSIFIEDS_PARTS_COUNT, sizeof(PartMetadata_t));
    if (!result) return NULL;

    const uint8_t* ptr = data + CLASSIFIEDS_PARTS_START;
    for (int i = 0; i < CLASSIFIEDS_PARTS_COUNT; i++) {
        if (!ParseClassifiedEntry(data, dataSize, ptr, result[i].classified)) {
            goto cleanup;
        }
    }

    return result;

cleanup:
    for (int i = 0; i < CLASSIFIEDS_PARTS_COUNT; i++) {
        SrLibDataFreeClassified(&result[i].classified);
    }
    free(result);
    *outCount = 0;
    return NULL;
}

// colors: the sprite's colours 1..3 in Amiga format. out_buffer is width*height RGBA.
int SrLibAmigaSpriteToRGBA(const uint16_t* data,
                           int startOffset,
                           const uint16_t* colors,
                           int width,
                           int height,
                           SrLibColor* out_buffer) {


    const uint8_t* byteData = reinterpret_cast<const uint8_t*>(data) + startOffset;

    for (int y = 0; y < height; y++) {
        // DATA and DATB words, big-endian.
        uint16_t data_word = (byteData[0] << 8) | byteData[1];
        uint16_t datb_word = (byteData[2] << 8) | byteData[3];
        byteData += 4;

        for (int x = 0; x < width; x++) {
            int bit_pos = 15 - x;
            int data_bit = (data_word >> bit_pos) & 1;
            int datb_bit = (datb_word >> bit_pos) & 1;
            int color_idx = (datb_bit << 1) | data_bit;  // Combine bits to get 0-3 value


            SrLibColor* pixel = &out_buffer[y * width + x];
            if (color_idx == 0) {
                // Colour 0 is transparent.
                pixel->r = pixel->g = pixel->b = pixel->a = 0;
            } else {
                SrLibColor rgb_color = BitmapParser::GetRGBFromRGB4(colors[color_idx - 1]);
                pixel->r = rgb_color.r;
                pixel->g = rgb_color.g;
                pixel->b = rgb_color.b;
                pixel->a = 255;
            }
        }
    }

    return 0;
}

int SrLibAmigaSpriteReadMousePointer(const uint16_t* data, SrLibAmigaSpriteAsset sprite, SrLibColor* out_buffer) {
    SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[sprite];
    return SrLibAmigaSpriteToRGBA(data, info.offset, info.colors, info.width, info.height, out_buffer);
}

CarMetadata_t* DataParser::SrLibDataReadDataFromFile(const char* filename, int* outCount) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "File error: Unable to open file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, size, file) != size) {
        fprintf(stderr, "File error: Unable to read file %s\n", filename);
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);

    CarMetadata_t* result = SrLibDataReadDataFromBuffer(buffer, size, outCount);
    free(buffer);
    return result;
}

void SrLibDataFreeClassified(ClassifiedEntry_t* classified) {
    if (classified) {
        free(classified->carName);
        free(classified->adText);
        classified->carName = nullptr;
        classified->adText = nullptr;
    }
}

int DataParser::GetCarIndex(AmigaLibAsset asset) {
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        if (kCarIndex[i] == (int)asset) {
            return i;
        }
    }
    return -1;
}

bool CarQuery::SrLibQueryCanChopRoof(AmigaLibAsset carAsset) {
    switch (carAsset) {
        case CHEVROLET_CORVETTE_1956_BODY:
        case CHEVROLET_CORVETTE_1961_BODY:
        case CHEVROLET_CORVETTE_1963_BODY:
        case THE_KING_CHEVROLET_CORVETTE_1963_BODY:
            return false;
        default:
            return true;
    }
}

bool CarQuery::SrLibQueryCanStripBumpers(AmigaLibAsset carAsset) {
    // The only three bodies that ship no CHOPPED_REARBUMPER/FRONTBUMPER pair.
    switch (carAsset) {
        case FORD_DEUCE_COUPE_1932_BODY:
        case FORD_SEDAN_1932_BODY:
        case THE_KING_CHEVROLET_CORVETTE_1963_BODY:
            return false;
        default:
            return true;
    }
}

// Bottom-aligned against the sprite height.
int SrLibQueryGetCarRoofYOffset(SrLibGameScene scene, int spriteHeight, SrGame game) {
    // SR1 sinks the tires 7px behind the road edge that starts at y=155
    // FIXME:SR2's Y value here is guesswork
    if (scene == SCENE_DINER) return (game == SrGame::SR2 ? 172 : 162) - spriteHeight;
    if (scene == SCENE_GAS_STATION) return (game == SrGame::SR2 ? 184 : 181) - spriteHeight;
    return 151 - spriteHeight; // SCENE_GARAGE (and default for any other scene)
}

int SrLibQueryGetCarIdleX(SrLibGameScene scene) {
    for (size_t i = 0; i < CAR_IDLE_X_COUNT; i++) {
        if (CAR_IDLE_X[i][0] == scene) {
            return CAR_IDLE_X[i][1];
        }
    }
    return 35; // Default X position if scene not found
}


// The chopped-roof slot only exists for choppable bodies, so the Corvettes' bumper overlays sit one earlier.
static int BumperOverlayBase(AmigaLibAsset carAsset) {
    return static_cast<int>(carAsset) +
           (CarQuery::SrLibQueryCanChopRoof(carAsset) ? 2 : 1);
}

int CarQuery::SrLibQueryGetChoppedBumberRearIndex(AmigaLibAsset carAsset) {
    return BumperOverlayBase(carAsset);
}

int CarQuery::SrLibQueryGetChoppedBumberFrontIndex(AmigaLibAsset carAsset) {
    return BumperOverlayBase(carAsset) + 1;
}

CarVendor_t SrLibQueryGetCarVendor(int carIndex) {
     for (size_t i = 0; i < CAR_VENDOR_LOOKUP_COUNT; i++) {
        if (CAR_VENDOR_LOOKUP[i][0] == carIndex) {
            return (CarVendor_t)CAR_VENDOR_LOOKUP[i][1];
        }
    }

    return CarVendorUnknown;
}

uint32_t SrLibCalculateChecksum(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    uint32_t checksum = 0;
    uint8_t buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            checksum = ((checksum << 5) + checksum) + buffer[i];
        }
    }

    fclose(file);
    return checksum;
}

SrLibStatus SrLibVerifyChecksum(const char *filename, uint32_t expected_checksum) {
    uint32_t actual_checksum = SrLibCalculateChecksum(filename);
    if (actual_checksum == 0) {
        printf("Error: Could not open file %s\n", filename);
        return SrLibStatusFileNotFound;
    }

    if (actual_checksum != expected_checksum) {
        printf("Signature mismatch for %s\n", filename);
        printf("Expected: %08X\n", expected_checksum);
        printf("Actual:   %08X\n", actual_checksum);
        return SrLibStatusChecksumMismatch;
    }

    return SrLibStatusSuccess;
}

bool TrackReader::LoadTrackData(const std::string& binaryPath) {
    std::ifstream file(binaryPath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    binaryData.resize(fileSize);
    file.read(reinterpret_cast<char*>(binaryData.data()), fileSize);
    file.close();

    dataLoaded = true;
    return true;
}

bool TrackReader::LoadTrackData(const std::vector<uint8_t>& binaryData) {
    this->binaryData = binaryData;
    dataLoaded = !binaryData.empty();
    return dataLoaded;
}

std::vector<SrTrackCoordSegment> TrackReader::ExtractTrackCoordinates(SrTrackType trackType, int maxSegments) {
    std::vector<SrTrackCoordSegment> segments;

    if (!dataLoaded || binaryData.empty()) {
        return segments;
    }

    uint32_t offset = (trackType == SR_TRACK_ROAD_RACE) ? SR_TRACK_ROAD_RACE_OFFSET : SR_TRACK_DRAG_RACE_OFFSET;

    if (offset + 12 > binaryData.size()) {
        return segments;
    }

    for (int i = 0; i < maxSegments; ++i) {
        size_t segmentOffset = offset + (i * 12); // 12 bytes per segment

        if (segmentOffset + 12 > binaryData.size()) {
            break;
        }

        SrTrackCoordSegment segment;
        segment.xStart = ReadBigEndian16(binaryData.data(), segmentOffset);
        segment.xEnd = ReadBigEndian16(binaryData.data(), segmentOffset + 2);
        segment.yStart = ReadBigEndian16(binaryData.data(), segmentOffset + 4);
        segment.yEnd = ReadBigEndian16(binaryData.data(), segmentOffset + 6);
        segment.sprite1 = ReadBigEndian16(binaryData.data(), segmentOffset + 8);
        segment.sprite2 = ReadBigEndian16(binaryData.data(), segmentOffset + 10);

        ScaleCoordinates(segment);

        if (std::abs(static_cast<int>(segment.xStart)) > 400 || std::abs(static_cast<int>(segment.xEnd)) > 400 ||
            std::abs(static_cast<int>(segment.yStart)) > 400 || std::abs(static_cast<int>(segment.yEnd)) > 400) {
            break;
        }

        segments.push_back(segment);
    }

    return segments;
}

std::vector<SrRoadSideSpriteRecord> TrackReader::ExtractRoadSideSprites(uint32_t offset, int maxRecords) {
    std::vector<SrRoadSideSpriteRecord> sprites;

    if (!dataLoaded || binaryData.empty()) {
        return sprites;
    }

    if (offset + 8 > binaryData.size()) {
        return sprites;
    }

    for (int i = 0; i < maxRecords; ++i) {
        size_t recordOffset = offset + (i * 8); // 8 bytes per record

        if (recordOffset + 8 > binaryData.size()) {
            break;
        }

        SrRoadSideSpriteRecord record;
        record.spriteId1 = ReadBigEndianU16(binaryData.data(), recordOffset);
        record.trackPosition1 = ReadBigEndianU16(binaryData.data(), recordOffset + 2);
        record.spriteId2 = ReadBigEndianU16(binaryData.data(), recordOffset + 4);
        record.trackPosition2 = ReadBigEndianU16(binaryData.data(), recordOffset + 6);

        ScaleSpritePosition(record);

        if (record.trackPosition1 > 5000 || record.trackPosition2 > 5000) {
            break;
        }

        sprites.push_back(record);
    }

    return sprites;
}

const char* TrackReader::GetSpriteName(uint16_t spriteId) {
    switch (spriteId) {
        case SR_SPRITE_RACING_CLOUDS: return "Clouds";
        case SR_SPRITE_RACING_MOUNTAINS: return "Mountains";
        case SR_SPRITE_RACING_SIGN_COUNTY_LINE: return "Country Line Sign";
        case SR_SPRITE_RACING_SIGN_GARAGE: return "Garage Sign";
        case SR_SPRITE_RACING_SIGN_GUS_GAS: return "Gus Gas Sign";
        case SR_SPRITE_RACING_DRIVE_IN: return "Drive-In";
        case SR_SPRITE_RACING_PALM_TREE: return "Palm Tree";
        case SR_SPRITE_RACING_ROAD_POSTS: return "Road Posts (alt)";
        case SR_SPRITE_RACING_SIGN_ROAD_WORK_AHEAD: return "Road Work Ahead Sign";
        case SR_SPRITE_RACING_BUSH: return "Bush";
        case SR_SPRITE_RACING_TELEPHONE_POLE: return "Telephone Pole";
        case SR_SPRITE_RACING_TREE_31: return "Tree 31";
        case SR_SPRITE_RACING_TREE_32: return "Tree 32";
        case SR_SPRITE_RACING_ROAD_POSTS_33: return "Road Posts";
        case SR_SPRITE_RACING_UNKNOWN_18: return "Unknown 18";
        case SR_SPRITE_RACING_UNKNOWN_19: return "Unknown 19";
        case SR_SPRITE_RACING_UNKNOWN_20: return "Unknown 20";
        default: return "Unknown Sprite";
    }
}

TrackReader::TrackAnalysis TrackReader::AnalyzeTrack(const std::vector<SrTrackCoordSegment>& segments) {
    TrackAnalysis analysis = {};
    analysis.segmentCount = segments.size();

    if (segments.empty()) {
        return analysis;
    }

    analysis.minX = analysis.maxX = static_cast<int>(segments[0].xStart);
    analysis.minY = analysis.maxY = static_cast<int>(segments[0].yStart);

    std::set<uint16_t> spriteSet;

    for (const auto& segment : segments) {
        analysis.minX = std::min({analysis.minX, static_cast<int>(segment.xStart), static_cast<int>(segment.xEnd)});
        analysis.maxX = std::max({analysis.maxX, static_cast<int>(segment.xStart), static_cast<int>(segment.xEnd)});
        analysis.minY = std::min({analysis.minY, static_cast<int>(segment.yStart), static_cast<int>(segment.yEnd)});
        analysis.maxY = std::max({analysis.maxY, static_cast<int>(segment.yStart), static_cast<int>(segment.yEnd)});

        int dx = segment.xEnd - segment.xStart;
        int dy = segment.yEnd - segment.yStart;
        float length = sqrt(dx * dx + dy * dy);
        analysis.totalLength += length;

        if (dx != 0 || dy != 0) {
            analysis.curvedSegments++;
        } else {
            analysis.straightSegments++;
        }

        if (segment.sprite1 != 0) spriteSet.insert(segment.sprite1);
        if (segment.sprite2 != 0) spriteSet.insert(segment.sprite2);
    }

    analysis.uniqueSprites.assign(spriteSet.begin(), spriteSet.end());

    return analysis;
}

std::vector<SrDriverData> DataParser::ParseDriverData(const std::vector<uint8_t>& sr_data) {
    std::vector<SrDriverData> result;
    const size_t block_size = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    if (sr_data.size() < block_size) return result;

    result.reserve(SR_DATA_DRIVER_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_DRIVER_ENTRY_COUNT; ++i) {
        SrDriverData entry;
        memcpy(&entry, sr_data.data() + i * sizeof(SrDriverData), sizeof(SrDriverData));
        result.push_back(entry);
    }
    return result;
}

std::vector<std::string> DataParser::ParseDriverNames(const std::vector<uint8_t>& sr_data) {
    std::vector<std::string> names;
    const size_t driver_block = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    const size_t carspec_block = SR_DATA_CARSPEC_ENTRY_COUNT * SR_DATA_CARSPEC_RECORD_SIZE;
    if (sr_data.size() < driver_block + carspec_block) return names;

    // The first car spec's ad text is the start of the blob, recovering the pointer base.
    SrCarSpecEntry firstSpec;
    memcpy(&firstSpec, sr_data.data() + driver_block, SR_DATA_CARSPEC_RECORD_SIZE);
    const uint32_t base = firstSpec.AdTextPtr() - SR_DATA_TEXT_BLOB_OFFSET;

    names.reserve(SR_DATA_DRIVER_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_DRIVER_ENTRY_COUNT; ++i) {
        SrDriverData entry;
        memcpy(&entry, sr_data.data() + i * sizeof(SrDriverData), sizeof(SrDriverData));
        const uint32_t ptr = entry.NamePtr();
        std::string name;
        if (ptr >= base) {
            size_t off = ptr - base;
            while (off < sr_data.size() && sr_data[off] != 0 && name.size() < 32) {
                name.push_back(static_cast<char>(sr_data[off]));
                ++off;
            }
        }
        names.push_back(name);
    }
    return names;
}

static uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// The LIB2 index plus HOT_DATA_SPRITE_BASE; 0 = no such variant.
static int DosSpriteRef(const uint8_t* p) {
    uint16_t raw = ReadU16LE(p);
    return raw ? static_cast<int>(raw) - HOT_DATA_SPRITE_BASE : -1;
}

static bool DosHotDataHasTables(size_t dataSize) {
    return dataSize >= HOT_DATA_TEXT_START;
}

std::vector<SrCarSpecEntry> DataParser::ParseCarSpecTableDOS(const std::vector<uint8_t>& hot_data) {
    std::vector<SrCarSpecEntry> result;
    if (!DosHotDataHasTables(hot_data.size())) return result;

    result.reserve(HOT_DATA_CAR_COUNT);
    for (int i = 0; i < HOT_DATA_CAR_COUNT; ++i) {
        const uint8_t* rec = hot_data.data() + HOT_DATA_CARSPEC_START + i * 10;
        const uint16_t price = ReadU16LE(rec);
        const uint8_t partsA = rec[4];
        const uint8_t partsB = rec[5];

        // Repacked into the Amiga's config word so every SrCarSpecEntry accessor works.
        const int brandMask = partsA & 0x0F;         // 1=GM, 2=Ford, 4=Chrysler
        const int tires     = (partsA >> 4) & 0x03;
        const int trans     = (partsA >> 6) & 0x03;
        const int engine    = partsB & 0x07;
        const int manifold  = (partsB >> 3) & 0x07;
        const int carb      = (partsB >> 6) & 0x03;
        const int config    = (brandMask << 12) | (tires << 10) | (trans << 8) |
                              (carb << 6) | (manifold << 3) | engine;

        // Absolute file offset, as on the Amiga.
        const uint32_t adPtr = static_cast<uint32_t>(HOT_DATA_TEXT_START) + ReadU16LE(rec + 6);

        SrCarSpecEntry entry = {};
        entry.price[0] = static_cast<uint8_t>(price >> 8);
        entry.price[1] = static_cast<uint8_t>(price & 0xFF);
        entry.car_class = rec[2];
        entry.config[0] = static_cast<uint8_t>(config >> 8);
        entry.config[1] = static_cast<uint8_t>(config & 0xFF);
        entry.ad_text_ptr[0] = static_cast<uint8_t>(adPtr >> 24);
        entry.ad_text_ptr[1] = static_cast<uint8_t>(adPtr >> 16);
        entry.ad_text_ptr[2] = static_cast<uint8_t>(adPtr >> 8);
        entry.ad_text_ptr[3] = static_cast<uint8_t>(adPtr & 0xFF);
        // body is a one-based LIB2 sprite index here.
        const int body = ReadU16LE(rec + 8) - HOT_DATA_SPRITE_BASE + 1;
        entry.body[0] = static_cast<uint8_t>((body >> 8) & 0xFF);
        entry.body[1] = static_cast<uint8_t>(body & 0xFF);
        result.push_back(entry);
    }
    return result;
}

// SR2's car spec record, 20 bytes big-endian:
//   +0 price  +2 a second figure  +4 class  +6 brand mask  +8 config
//   +10 ad pointer  +14 ad lines  +16 body asset  +18 a per-car figure
// See RodLegacyResearch/docs/formats/sr_data.md, "SR2_data".
std::vector<SrCarSpecEntry> DataParser::ParseCarSpecTableSR2(const std::vector<uint8_t>& sr2_data) {
    std::vector<SrCarSpecEntry> result;
    const size_t driverBlock = SR2_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    const size_t specBlock = static_cast<size_t>(SR2_DATA_CARSPEC_ENTRY_COUNT) *
                             SR2_DATA_CARSPEC_RECORD_SIZE;
    if (sr2_data.size() < driverBlock + specBlock) return result;

    result.reserve(SR2_DATA_CARSPEC_ENTRY_COUNT);
    for (int i = 0; i < SR2_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        const uint8_t* rec = sr2_data.data() + driverBlock +
                             static_cast<size_t>(i) * SR2_DATA_CARSPEC_RECORD_SIZE;
        auto be16 = [](const uint8_t* p) { return (p[0] << 8) | p[1]; };

        // SR2 gave the brand its own field and reused bits 12..15 of the config word for the exhaust, muffler
        // and differential it added.
        const int brandMask = be16(rec + 6) & 0x07;   // 1=GM, 2=Ford, 4=Chrysler
        const int cfg       = be16(rec + 8);
        const int engine    = cfg & 0x03;
        const int manifold  = (cfg >> 2) & 0x0F;
        const int carb      = (cfg >> 6) & 0x03;
        const int trans     = (cfg >> 8) & 0x03;
        const int tires     = (cfg >> 10) & 0x03;
        // The shipped data only uses manifold classes 0, 1 and 4, so nothing
        // overflows SR1's three-bit field. Guard it anyway.
        const int packed = (brandMask << 12) | (tires << 10) | (trans << 8) |
                           (carb << 6) | ((manifold & 0x07) << 3) | engine;

        SrCarSpecEntry entry = {};
        // Bits 12..15 are the three parts SR2 added.
        entry.exhaust      = static_cast<uint8_t>((cfg >> 12) & 0x01);
        entry.muffler      = static_cast<uint8_t>((cfg >> 13) & 0x01);
        entry.differential = static_cast<uint8_t>((cfg >> 14) & 0x03);
        entry.price[0] = rec[0];
        entry.price[1] = rec[1];
        entry.car_class = rec[4];
        entry.config[0] = static_cast<uint8_t>(packed >> 8);
        entry.config[1] = static_cast<uint8_t>(packed & 0xFF);
        memcpy(entry.ad_text_ptr, rec + 10, 4);
        entry.ad_lines = rec[14];
        entry.body[0] = rec[16];
        entry.body[1] = rec[17];
        result.push_back(entry);
    }
    return result;
}

std::vector<std::string> DataParser::ParseDriverNamesSR2(const std::vector<uint8_t>& sr2_data) {
    std::vector<std::string> names;
    const size_t driverBlock = SR2_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    const size_t specBlock = static_cast<size_t>(SR2_DATA_CARSPEC_ENTRY_COUNT) *
                             SR2_DATA_CARSPEC_RECORD_SIZE;
    if (sr2_data.size() < driverBlock + specBlock) return names;

    // Same trick as SR1: the first car's ad pointer is the start of the text
    // blob, which recovers the load base every absolute pointer is relative to.
    const uint8_t* firstSpec = sr2_data.data() + driverBlock;
    const uint32_t firstAd = (static_cast<uint32_t>(firstSpec[10]) << 24) |
                             (static_cast<uint32_t>(firstSpec[11]) << 16) |
                             (static_cast<uint32_t>(firstSpec[12]) << 8) |
                              static_cast<uint32_t>(firstSpec[13]);
    const uint32_t base = firstAd - SR2_DATA_TEXT_BLOB_OFFSET;

    names.reserve(SR2_DATA_DRIVER_ENTRY_COUNT);
    for (int i = 0; i < SR2_DATA_DRIVER_ENTRY_COUNT; ++i) {
        SrDriverData entry;
        memcpy(&entry, sr2_data.data() + i * sizeof(SrDriverData), sizeof(SrDriverData));
        const uint32_t ptr = entry.NamePtr();
        std::string name;
        if (ptr >= base) {
            size_t off = ptr - base;
            while (off < sr2_data.size() && sr2_data[off] != 0 && name.size() < 32) {
                name.push_back(static_cast<char>(sr2_data[off]));
                ++off;
            }
        }
        names.push_back(name);
    }
    return names;
}

std::vector<CarSpriteRefs> DataParser::ParseCarSpritesDOS(const std::vector<uint8_t>& hot_data) {
    std::vector<CarSpriteRefs> result;
    if (!DosHotDataHasTables(hot_data.size())) return result;

    result.reserve(HOT_DATA_CAR_COUNT);
    for (int i = 0; i < HOT_DATA_CAR_COUNT; ++i) {
        const uint8_t* spec = hot_data.data() + HOT_DATA_CARSPEC_START + i * 10;
        const uint8_t* rec = hot_data.data() + HOT_DATA_CARSPRITES_START + i * 10;
        CarSpriteRefs sprites;
        sprites.body = DosSpriteRef(spec + 8);
        // rec[0..1] is a 1..4 value, meaning unknown.
        sprites.choppedRoof = DosSpriteRef(rec + 2);
        sprites.airScoop = DosSpriteRef(rec + 4);
        sprites.rearBumperStripped = DosSpriteRef(rec + 6);
        sprites.frontBumperStripped = DosSpriteRef(rec + 8);
        result.push_back(sprites);
    }
    return result;
}

CarMetadata_t* DataParser::SrLibDataReadDataFromBufferDOS(const uint8_t* data, size_t dataSize, int* outCount) {
    if (!data || !outCount) return NULL;
    *outCount = 0;
    if (!DosHotDataHasTables(dataSize)) return NULL;

    CarMetadata_t* result = (CarMetadata_t*)calloc(CAR_PLAYER_MODEL_COUNT, sizeof(CarMetadata_t));
    if (!result) return NULL;

    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        const uint8_t* ptr = data + HOT_DATA_OFFSETS_START + i * 12;
        BitmapOffset_t offsets = {};
        offsets.offsetXRearTire = *ptr++;
        offsets.offsetXFrontTire = *ptr++;
        offsets.offsetXDriver = *ptr++;
        offsets.offsetYDriver = *ptr++;
        offsets.offsetXChoppedRoof = *ptr++;
        offsets.offsetXAirScoop = *ptr++;
        offsets.offsetYAirScoop = *ptr++;
        offsets.offsetYRearBumper = *ptr++;
        offsets.offsetXFrontBumper = *ptr++;
        offsets.offsetYFrontBumper = *ptr++;
        offsets.offsetXDecal = *ptr++;
        offsets.offsetYDecal = *ptr++;

        // The gas cap is in the gas station table; the caller fills offsetYTires.
        const uint8_t* gas = data + HOT_DATA_GASSTATION_START + i * 6;
        offsets.offsetXGasCap = gas[0];
        offsets.offsetYGasCap = gas[1];

        result[i].offsets = offsets;
    }

    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        const uint8_t* ptr = data + HOT_DATA_CLICKBOX_START + i * 20;
        CarClickBoxes_t boxes = {};
        ClickBox_t* order[] = {
            &boxes.clickBoxRearBumper,
            &boxes.clickBoxChoppedRoof,
            &boxes.clickBoxTransmission,
            &boxes.clickBoxEngine,
            &boxes.clickBoxFrontBumper,
        };
        for (ClickBox_t* box : order) {
            box->x = *ptr++;
            box->y = *ptr++;
            box->width = *ptr++;
            box->height = *ptr++;
        }

        const uint8_t* gas = data + HOT_DATA_GASSTATION_START + i * 6;
        boxes.clickBoxGasCap = {gas[0], gas[1], 12, 12};
        boxes.clickBoxRearWindow = {gas[2], gas[3], 24, 24};
        boxes.clickBoxFrontWindow = {gas[4], gas[5], 24, 24};

        result[i].clickBoxes = boxes;
    }

    // Each car names its own ad offset; reading straight through drifts on padding.
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        const size_t adStart = HOT_DATA_TEXT_START +
                               ReadU16LE(data + HOT_DATA_CARSPEC_START + i * 10 + 6);
        const uint8_t* ptr = data + adStart;
        if (adStart >= dataSize ||
            !ParseClassifiedEntry(data, dataSize, ptr, result[i].classified)) {
            for (int j = 0; j < i; j++) {
                SrLibDataFreeClassified(&result[j].classified);
            }
            free(result);
            return NULL;
        }
    }

    *outCount = CAR_PLAYER_MODEL_COUNT;
    return result;
}

std::vector<SrCarSpecEntry> DataParser::ParseCarSpecTable(const std::vector<uint8_t>& sr_data) {
    std::vector<SrCarSpecEntry> result;
    const size_t driver_block = SR_DATA_DRIVER_ENTRY_COUNT * sizeof(SrDriverData);
    const size_t carspec_block = SR_DATA_CARSPEC_ENTRY_COUNT * SR_DATA_CARSPEC_RECORD_SIZE;
    if (sr_data.size() < driver_block + carspec_block) return result;

    result.reserve(SR_DATA_CARSPEC_ENTRY_COUNT);
    for (int i = 0; i < SR_DATA_CARSPEC_ENTRY_COUNT; ++i) {
        SrCarSpecEntry entry;
        memcpy(&entry, sr_data.data() + driver_block + i * SR_DATA_CARSPEC_RECORD_SIZE,
               SR_DATA_CARSPEC_RECORD_SIZE);
        result.push_back(entry);
    }
    return result;
}
