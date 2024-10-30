#include "libp.h"
#include "carcolor.h"
#include <fstream>
#include <iostream>
#include <SDL.h>

using namespace std;


uint16_t ReadUint16BigEndian(std::ifstream& file) {
    uint8_t bytes[2];
    file.read(reinterpret_cast<char*>(bytes), 2);
    return (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
}

uint32_t ReadUint32BigEndian(std::ifstream& file) {
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

uint8_t ReadUint8(std::ifstream& file) {
    uint8_t byte;
    file.read(reinterpret_cast<char*>(&byte), 1);
    return byte;
}

void BitmapParser::PrintBitmapEntry(int number, BitmapEntry_t entry) {
    printf("---------\n");
    printf("bitmap %d\n", number);
    printf("width %d\n", entry.width);
    printf("height %d\n", entry.height);
    printf("depth %d\n", entry.depth);
    printf("unknown1 %d\n", entry.unknown1);
    printf("offset %d\n", entry.offset);
    printf("sizePacked %d\n", entry.sizePacked);
}

void BitmapParser::PrintBitplanes(const std::vector<uint16_t>& data) {
    const int bytesPerColumn = 4;
    const int maxColumns = 8;

    for (size_t i = 0; i < data.size(); ++i) {
        // Print each byte as a two-digit hexadecimal number
        std::cout << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(data[i]);

        // Insert a space after every byte
        if ((i + 1) % bytesPerColumn == 0) {
            std::cout << ' ';
        }

        // Insert a newline after every maxColumns * bytesPerColumn bytes
        if ((i + 1) % (bytesPerColumn * maxColumns) == 0) {
            std::cout << std::endl;
        }
    }

    // Handle the case where the last row may not be completely filled
    if (data.size() % (bytesPerColumn * maxColumns) != 0) {
        std::cout << std::endl;
    }
}

pair<int, vector<BitmapEntry_t>> BitmapParser::GetEntries(const string& filename) {
    vector<BitmapEntry_t> result;
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return pair<int, vector<BitmapEntry_t>>(0, result);
    }

    uint16_t totalEntriesCount = ReadUint16BigEndian(file);
    // cout << "Number of bitmap entries in " << filename << ": " << totalEntriesCount << endl;

    for (int i = 0; i < totalEntriesCount; ++i) {
        BitmapEntry_t newEntry = {};
        newEntry.width = ReadUint16BigEndian(file);
        newEntry.height = ReadUint16BigEndian(file);
        newEntry.depth = ReadUint8(file);
        newEntry.unknown1 = ReadUint8(file);
        file.read(reinterpret_cast<char*>(newEntry.rleDictionary), sizeof(newEntry.rleDictionary));
        newEntry.offset = ReadUint32BigEndian(file);
        newEntry.sizePacked = ReadUint16BigEndian(file);

        // PrintBitmapEntry(i, newEntry);
        result.push_back(newEntry);
    }

    return pair<int, vector<BitmapEntry_t>>(totalEntriesCount, result);
}

vector<uint8_t> BitmapParser::GetPackedData(const std::string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("File error: Unable to open file");
    }

    file.seekg(0, ios::end);
    int fileSize = file.tellg();
    file.seekg(0, ios::beg);

    int fileOffset = 2 + (sizeof(BitmapEntry_t) * totalEntriesCount) + bitmapEntry.offset;
    // cout << "entries " << totalEntriesCount << endl;
    // cout << "sizeof " << sizeof(BitmapEntry_t) << endl;
    // cout << "Offset for bitmap " << bitmapEntry.offset << " in file " << fileOffset << endl;

    if (fileOffset >= fileSize) {
        throw runtime_error("Can't read data because offset is larger than file size.");
    }

    file.seekg(fileOffset, ios::beg);
    vector<uint8_t> buf(bitmapEntry.sizePacked);
    file.read(reinterpret_cast<char*>(buf.data()), bitmapEntry.sizePacked);

    if (!file) {
        throw runtime_error("Error reading file");
    }

    return buf;
}

vector<uint8_t> BitmapParser::UnpackData(const vector<pair<int, uint8_t>> &rle, const BitmapEntry_t &sprite, const vector<uint8_t> &data) {
    int p = 0;
    int width = sprite.width / 8;
    int height = sprite.height;
    int depth = sprite.depth;
    int size_expected = (sprite.width * sprite.height * sprite.depth) / 8;

    vector<uint8_t> flat_bp;

    int pos = 0;

    if (rle.size() != 16) {
        cout << "No valid RLE table!" << endl;
        return flat_bp;
    }

    while (pos < width * height * depth) {
        uint8_t b = data[p];

        bool found = false;
        const uint8_t *directory = sprite.rleDictionary;

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
        // Pad data at end
        int missing = size_expected - flat_bp.size();
        flat_bp.insert(flat_bp.end(), missing, 0x0);
    }

    return flat_bp;
}

std::vector<Color> BitmapParser::PlanarToChunky(const std::vector<uint8_t>& data, BitmapEntry_t entry, const Palette& palette, const Palette& carPalette) {
    std::vector<Color> chunkyData(entry.width * entry.height);

    int planeSize = (entry.width / 8) * entry.height;

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

                // Select palette based on value range
                const Palette& selectedPalette = ((value >= 1 && value <= 9 || (value >= 1+32 && value <= 9+32)) && !carPalette.empty()) ? carPalette : palette;
                int pixel_index = (y * entry.width) + (x + i);
                chunkyData[pixel_index] = selectedPalette[value];
            }
        }
    }

    SavePaletteToBMP(palette, "normalpalette.bmp");
    SavePaletteToBMP(carPalette, "carcolors.bmp");

    // std::cout << "chunky data size: " << chunkyData.size() << std::endl;
    // Optionally print or process chunkyData further

    return chunkyData;
}

// Converts 12bit palette to RGB and adds EHB colors
Palette BitmapParser::PreparePalette(const vector<uint16_t>& palette, bool ehb) {
    Palette rval;

    // Add RGB colors from the palette
    for (int i = 0; i < palette.size(); i++) {
        rval.push_back(GetRGBFromRGB4(palette[i]));
    }

    if (ehb) {
        // Add EHB colors
        for (int i = 0; i < palette.size(); i++) {
            Color color = rval[i];
            uint8_t r = get<0>(color);
            uint8_t g = get<1>(color);
            uint8_t b = get<2>(color);
            rval.push_back(make_tuple((r / 2) & 0xF0, (g / 2) & 0xF0, (b / 2) & 0xF0));
        }
    }

    // Print the palette colors
    for (int i = 0; i < rval.size(); i++) {
        Color color = rval[i];
        uint8_t r = get<0>(color);
        uint8_t g = get<1>(color);
        uint8_t b = get<2>(color);
        // cout << "Palette " << i << ": " << static_cast<int>(r) << "," << static_cast<int>(g) << "," << static_cast<int>(b) << endl;
    }

    return rval;
}

// Creates a black palette except for color 1-10 (car shades) and EHB counterparts
Palette BitmapParser::PrepareCarPalette(const vector<uint16_t>& palette) {
    Palette rval;

    // Add RGB colors from the palette
    for (int i = 0; i < 32; i++) {
        rval.push_back(make_tuple(0,0,0));
    }

    for (int i = 0; i < 9; i++) {
        rval.at(i+1) = GetRGBFromRGB4(palette[i]);
    }

    // Add EHB colors
    for (int i = 0; i < palette.size(); i++) {
        Color color = rval[i];
        uint8_t r = get<0>(color);
        uint8_t g = get<1>(color);
        uint8_t b = get<2>(color);
        rval.push_back(make_tuple((r / 2) & 0xF0, (g / 2) & 0xF0, (b / 2) & 0xF0));
    }

    return rval;
}

void BitmapParser::SavePaletteToBMP(const Palette& palette, const std::string& filename) {
    // Calculate the size of the BMP image
    int BLOCK_SIZE = 16;
    int numColors = palette.size();
    int imageWidth = BLOCK_SIZE * numColors;
    int imageHeight = BLOCK_SIZE;


    // Create an SDL surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, imageWidth, imageHeight, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);
    if (!surface) {
        std::cerr << "Failed to create SDL surface: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    // Fill the surface with palette colors
    for (int i = 0; i < numColors; ++i) {
        Color c = palette[i];
        SDL_Rect rect = { i * BLOCK_SIZE, 0, BLOCK_SIZE, BLOCK_SIZE };
        SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, get<0>(c), get<1>(c), get<2>(c)));
    }

    // Save the surface as a BMP file
    if (SDL_SaveBMP(surface, filename.c_str()) != 0) {
        std::cerr << "Failed to save BMP file: " << SDL_GetError() << std::endl;
    }

    // Free the surface and quit SDL
    SDL_FreeSurface(surface);
}

Color BitmapParser::GetRGBFromRGB4(uint16_t rgb4) {
    uint8_t r = (rgb4 & 0xF00) >> 8;
    uint8_t g = (rgb4 & 0x0F0) >> 4;
    uint8_t b = (rgb4 & 0x00F);

    r = r << 4;
    g = g << 4;
    b = b << 4;

    return make_tuple(r, g, b);
}

vector<pair<BitmapEntry_t, vector<uint8_t>>> BitmapParser::ReadAllImages(const string& filename) {
    vector<pair<BitmapEntry_t, vector<uint8_t>>> result;
    pair<int, vector<BitmapEntry_t>> entries = GetEntries(filename);

    // for (int i = 0; i < entries.second.size(); i++) {
    //     auto b = entries.second[i];
    //     vector<uint8_t> imgData = ReadImage(filename, b, i, entries.first);
    //     result.push_back(make_pair(b, imgData));
    // }
    // Palette pal = p.PreparePalette(PAL1);
    // vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);

    return result;
}


vector<uint8_t> BitmapParser::ReadImage(const string& filename, BitmapEntry_t entry, int index, int totalEntriesCount) {
    int expectedSize = (entry.width * entry.height * entry.depth) / 8;
    vector<uint8_t> packedData = GetPackedData(filename, entry, totalEntriesCount);
    // cout << "packed data " << packedData.size()  << endl;
    // PrintBitplanes(packedData);
    vector<uint8_t> data = UnpackData(RLE, entry, packedData);
    if (expectedSize != data.size()) {
        cout << "Expected size: " << expectedSize << " actual size: " << data.size() << endl;
        //PrintBitplanes(data);
        throw runtime_error("Size mismatch reading bitmap");
    }
    return data;
}

LibAssetType BitmapParser::GetAssetType(AssetLocation location, int index) {
    switch (location) {
    case AssetLocationAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                case PHONEBOOK_GUY:
                case DECALS:
                    return LibAssetSpritesheet;
                case GUS_GAS_MASK:
                    return LibAssetMask;
                default:
                    return LibAssetRegular;
            }
    }

    default:
        return LibAssetRegular;
    }
}

// Return rows, columns for spritesheet
pair<int, int> BitmapParser::GetSpritesheetInfo(AssetLocation location, int index) {
switch (location) {
    case AssetLocationAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                    return make_tuple(1, 4);
                case PHONEBOOK_GUY:
                    return make_tuple(2, 5);
                case DECALS:
                    return make_tuple(2, 4);
                default:
                    return make_tuple(0, 0);
            }
    }

    default:
        return make_tuple(0, 0);
    }
}

vector<uint16_t> BitmapParser::GetPalette(AssetLocation location, int index) {
switch (location) {
    case AssetLocationAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case GUS_GAS_SCREEN:
                    return PAL_GAS;
                default:
                    return PAL1;
            }
    }

    default:
        return PAL1;
    }
}


vector<BitmapOffset_t> DataParser::GetEntries(const string& filename) {
    vector<BitmapOffset_t> offsets;
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return vector<BitmapOffset_t>();
    }

    #define OFFSETS_START 0x34C
    #define CARS_COUNT 17

    file.seekg(OFFSETS_START, ios::beg);

    for (int i = 0; i < CARS_COUNT; i++) {
        BitmapOffset_t newEntry;
        newEntry.offsetXRearTire = ReadUint8(file);
        newEntry.offsetXFrontTire = ReadUint8(file);
        newEntry.offsetXDriver = ReadUint8(file);
        newEntry.offsetYDriver = ReadUint8(file);
        newEntry.offsetXChoppedRoof = ReadUint8(file);
        newEntry.offsetXAirScoop = ReadUint8(file);
        newEntry.offsetYAirScoop = ReadUint8(file);
        newEntry.offsetYRearBumper = ReadUint8(file);
        newEntry.offsetXFrontBumper = ReadUint8(file);
        newEntry.offsetYFrontBumper = ReadUint8(file);
        newEntry.offsetXDecal = ReadUint8(file);
        newEntry.offsetYDecal = ReadUint8(file);
        offsets.push_back(newEntry);
    }

    return offsets;
}

int DataParser::GetCarIndex(AmigaLibAsset asset) {
    int stockCarCount = sizeof(CAR_INDEX);

    for (int i = 0; i < stockCarCount; i++) {
        if (CAR_INDEX[i] == (int)asset) {
            return i;
        }
    }
    return -1;
}
