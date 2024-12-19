#include "libp.h"

#include <SDL.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include "carcolor.h"

using namespace std;

const char *SrLibFileNames[] = {
    "lib.start",
    "lib.p",
    "LIB1",
    "LIB1C",
    "LIB2"
};

const uint32_t SrLibFileChecksums[] = {
    0x388A781C,
    0x3EBE203A,
    0x3460C66A,
    0x2DE22048,
    0xF3EAD7B0
};

const char *SrLibArchiveNames[] = {
    "SR1AMIGA_DISK1.adf",
    "SR1AMIGA_DISK2.adf",
    "SR2AMIGA_DISK1.adf",
    "SR2AMIGA_DISK2.adf",    
};

const uint32_t SrLibArchiveChecksums[] = {
    0x53587B9F,
    0xFF24276C,
    0x5458C0BC,
    0x135549EB
};

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

/// @brief Read metadata about bitmaps in library file
/// @param filename
/// @return Map of int (total entries) and BitmapEntry_t metadata struct
pair<int, vector<BitmapEntry_t>> BitmapParser::ReadEntries(const vector<uint8_t>& data) {
    vector<BitmapEntry_t> result;
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
        
        PrintBitmapEntry(i, newEntry);
        result.push_back(newEntry);
    }
    
    return pair<int, vector<BitmapEntry_t>>(totalEntriesCount, result);
}

vector<uint8_t> BitmapParser::GetPackedData(const vector<uint8_t>& data, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    int headerSize = 2 + (sizeof(BitmapEntry_t) * totalEntriesCount);
    int fileOffset = headerSize + bitmapEntry.offset;
    
    if (fileOffset + bitmapEntry.sizePacked > data.size()) {
        throw runtime_error("Can't read data because offset is larger than data size.");
    }
    
    return vector<uint8_t>(
        data.begin() + fileOffset,
        data.begin() + fileOffset + bitmapEntry.sizePacked
    );
}

vector<uint8_t> BitmapParser::UnpackData(const vector<pair<int, uint8_t>>& rle, const BitmapEntry_t& sprite, const vector<uint8_t>& data) {
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
                const Palette& selectedPalette = ((value >= 1 && value <= 9 || (value >= 1 + 32 && value <= 9 + 32)) && !carPalette.empty()) ? carPalette : palette;
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

// Converts 12bit palette to RGB and adds EHB colors needed for the Amiga graphics
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
        rval.push_back(make_tuple(0, 0, 0));
    }

    for (int i = 0; i < 9; i++) {
        rval.at(i + 1) = GetRGBFromRGB4(palette[i]);
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
        SDL_Rect rect = {i * BLOCK_SIZE, 0, BLOCK_SIZE, BLOCK_SIZE};
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
    pair<int, vector<BitmapEntry_t>> entries = ReadEntries(filename);

    // for (int i = 0; i < entries.second.size(); i++) {
    //     auto b = entries.second[i];
    //     vector<uint8_t> imgData = ReadImage(filename, b, i, entries.first);
    //     result.push_back(make_pair(b, imgData));
    // }
    // Palette pal = p.PreparePalette(PAL1);
    // vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);

    return result;
}

vector<uint8_t> BitmapParser::ReadImage(const vector<uint8_t>& data, BitmapEntry_t entry, int index, int totalEntriesCount) {
    vector<uint8_t> packedData = GetPackedData(data, entry, totalEntriesCount);
    return UnpackData(RLE, entry, packedData);
}

LibAssetType BitmapParser::GetAssetType(SrLibFile location, int index) {
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                case PHONEBOOK_GUY:
                case DECALS:
                case GUS_GAS_ATTENDANT:
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
pair<int, int> BitmapParser::GetSpritesheetInfo(SrLibFile location, int index) {
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                    return make_pair(1, 4);
                case PHONEBOOK_GUY:
                    return make_pair(2, 5);
                case DECALS:
                    return make_pair(2, 4);
                case GUS_GAS_ATTENDANT:
                    return make_pair(1, 4);
                default:
                    return make_pair(0, 0);
            }
        }

        default:
            return make_pair(0, 0);
    }
}

vector<uint16_t> BitmapParser::GetStockPalette(StockPalette palette) {
    switch (palette) {
        case StockPaletteAmigaDiner:
            return PAL1;
        case StockPaletteAmigaGasStation:
            return PAL_GAS;
        default:
            return PAL1;
    }
}

vector<uint16_t> BitmapParser::GetPalette(SrLibFile location, int index) {
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case GUS_GAS_SCREEN:
                case GUS_GAS_ATTENDANT:
                case GUS_GAS_PUMP1:
                case GUS_GAS_PUMP2:
                case GUS_GAS_PUMP3:
                case GUS_GAS_PUMP_FILLING:
                    return PAL_GAS;
                default:
                    return PAL1;
            }
        }

        default:
            return PAL1;
    }
}

// File-based wrapper for ReadEntries
pair<int, vector<BitmapEntry_t>> BitmapParser::ReadEntries(const string& filename) {
    // Read file into vector
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return make_pair(0, vector<BitmapEntry_t>());
    }
    
    // Get file size and create vector
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "File error: Unable to read file " << filename << endl;
        return make_pair(0, vector<BitmapEntry_t>());
    }
    
    // Use the new vector-based implementation
    return ReadEntries(buffer);
}

// File-based wrapper for GetPackedData
vector<uint8_t> BitmapParser::GetPackedData(const string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    // Read file into vector
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return vector<uint8_t>();
    }
    
    // Get file size and create vector
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "File error: Unable to read file " << filename << endl;
        return vector<uint8_t>();
    }
    
    // Use the new vector-based implementation
    return GetPackedData(buffer, bitmapEntry, totalEntriesCount);
}

// File-based wrapper for ReadImage
vector<uint8_t> BitmapParser::ReadImage(const string& filename, BitmapEntry_t entry, int index, int totalEntriesCount) {
    // Read file into vector
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return vector<uint8_t>();
    }
    
    // Get file size and create vector
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "File error: Unable to read file " << filename << endl;
        return vector<uint8_t>();
    }
    
    // Use the new vector-based implementation
    return ReadImage(buffer, entry, index, totalEntriesCount);
}

void DataParser::CleanupCarString(string& str) {
    size_t pos = 0;

    // Replace double backticks (``) with a space
    while ((pos = str.find("``", pos)) != string::npos) {
        str.replace(pos, 2, " ");  // Replace `` with space
    }

    pos = 0;  // Reset position

    // Replace single backticks (`) with a space
    while ((pos = str.find("`", pos)) != string::npos) {
        str.replace(pos, 1, " ");  // Replace ` with space
    }

    if (str[str.size() - 1] == '.') {
        str.pop_back();
    }
}

/// @brief Reads info about cars and parts from data file (SR_data)
/// @param filename
/// @return
vector<CarMetadata_t> DataParser::ReadEntries(const string& filename) {
    // Read file into vector
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return vector<CarMetadata_t>();
    }
    
    // Get file size and create vector
    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        cerr << "File error: Unable to read file " << filename << endl;
        return vector<CarMetadata_t>();
    }
    
    // Use the new vector-based implementation
    return ReadEntries(buffer);
}



/// @brief Reads info about cars and parts from data file (SR_data)
/// @param filename
/// @return

// CarMetadata_t[] SrLibReadDataAmiga(const uint8_t[] data)
vector<CarMetadata_t> DataParser::ReadEntries(const vector<uint8_t>& data) {
#define OFFSETS_START 0x34C
#define CLICKBOX_START 0x484 // ????
#define CLICKBOX_GASSTATION_START 0x68c
#define CLASSIFIEDS_START 0x854
    vector<CarMetadata_t> result;
    VectorReader reader(data);

    reader.Seek(OFFSETS_START);

    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarMetadata_t metadataEntry;
        BitmapOffset_t newEntry;
        newEntry.offsetXRearTire = reader.ReadUint8();
        newEntry.offsetXFrontTire = reader.ReadUint8();
        newEntry.offsetXDriver = reader.ReadUint8();
        newEntry.offsetYDriver = reader.ReadUint8();
        newEntry.offsetXChoppedRoof = reader.ReadUint8();
        newEntry.offsetXAirScoop = reader.ReadUint8();
        newEntry.offsetYAirScoop = reader.ReadUint8();
        newEntry.offsetYRearBumper = reader.ReadUint8();
        newEntry.offsetXFrontBumper = reader.ReadUint8();
        newEntry.offsetYFrontBumper = reader.ReadUint8();
        newEntry.offsetXDecal = reader.ReadUint8();
        newEntry.offsetYDecal = reader.ReadUint8();
        
        // We add supplemental offsets here,
        // even though they are not in the data file.        
        StockCarModels model = (StockCarModels)i;
        newEntry.offsetYTires = CAR_WHEELS_Y_OFFSETS[model][1];
        newEntry.offsetXGasCap = CAR_GAS_CAP_XY_OFFSETS[model][1];
        newEntry.offsetYGasCap = CAR_GAS_CAP_XY_OFFSETS[model][2];

        metadataEntry.offsets = newEntry;
        result.push_back(metadataEntry);
    }

    // Parse Classifieds
    reader.Seek(CLASSIFIEDS_START);
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        // Get a reference to modify the original entry, not a copy
        CarMetadata_t& metadataEntry = result[i];
        ClassifiedEntry_t newClassified;

        // Read car name
        string carNameStr = reader.ReadNullTerminatedString();
        string carYearStr = carNameStr.substr(0, 4);
        newClassified.year = stoi(carYearStr);
        CleanupCarString(carNameStr);
        newClassified.carName = carNameStr;

        // Read ad text
        string adText;
        while (true) {
            string line = reader.ReadNullTerminatedString();
            CleanupCarString(line);
            size_t dollarPos = line.find('$');
            if (line.empty() || dollarPos != string::npos) {
                // Convert price
                string priceStr = line.substr(dollarPos).substr(1);
                newClassified.price = stoi(priceStr);
                break;
            }
            if (!adText.empty()) {
                adText += " ";  // Add space between lines
            }
            adText += line;
        }
        newClassified.adText = adText;
        metadataEntry.classified = newClassified;
    }

    reader.Seek(CLICKBOX_START);
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarClickBoxes_t newClickBoxes;
        ClickBox_t rearBumper, choppedRoof, transmission, engine, frontBumper;
        rearBumper.x = reader.ReadUint8();
        rearBumper.y = reader.ReadUint8();
        rearBumper.width = reader.ReadUint8();
        rearBumper.height = reader.ReadUint8();
        newClickBoxes.clickBoxRearBumper = rearBumper;

        choppedRoof.x = reader.ReadUint8();
        choppedRoof.y = reader.ReadUint8();
        choppedRoof.width = reader.ReadUint8();
        choppedRoof.height = reader.ReadUint8();
        newClickBoxes.clickBoxChoppedRoof = choppedRoof;

        transmission.x = reader.ReadUint8();
        transmission.y = reader.ReadUint8();
        transmission.width = reader.ReadUint8();
        transmission.height = reader.ReadUint8();
        newClickBoxes.clickBoxTransmission = transmission;

        engine.x = reader.ReadUint8();
        engine.y = reader.ReadUint8();
        engine.width = reader.ReadUint8();
        engine.height = reader.ReadUint8();
        newClickBoxes.clickBoxEngine = engine;

        frontBumper.x = reader.ReadUint8();
        frontBumper.y = reader.ReadUint8();
        frontBumper.width = reader.ReadUint8();
        frontBumper.height = reader.ReadUint8();
        newClickBoxes.clickBoxFrontBumper = frontBumper;

        result[i].clickBoxes = newClickBoxes;
    }

    reader.Seek(CLICKBOX_GASSTATION_START);
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarMetadata_t& metadataEntry = result[i];
        CarClickBoxes_t& clickBoxes = metadataEntry.clickBoxes;
        clickBoxes.clickBoxGasCap.x = reader.ReadUint8();
        clickBoxes.clickBoxGasCap.y = reader.ReadUint8();
        clickBoxes.clickBoxGasCap.width = 12;
        clickBoxes.clickBoxGasCap.height = 12;
        
        clickBoxes.clickBoxRearWindow.x = reader.ReadUint8();
        clickBoxes.clickBoxRearWindow.y = reader.ReadUint8();
        clickBoxes.clickBoxRearWindow.width = 24;
        clickBoxes.clickBoxRearWindow.height = 24;

        clickBoxes.clickBoxFrontWindow.x = reader.ReadUint8();
        clickBoxes.clickBoxFrontWindow.y = reader.ReadUint8();
        clickBoxes.clickBoxFrontWindow.width = 24;
        clickBoxes.clickBoxFrontWindow.height = 24;
    }

    // (Optional) Output for debug
    for (const auto& c : result) {
        cout << "Car Name: " << c.classified.carName << endl;
        cout << "Ad Text: " << c.classified.adText << endl;
        cout << "Price: " << c.classified.price << endl << endl;

    }

    // todo return classifieds too

    return result;
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

bool CarQuery::CanChopRoof(AmigaLibAsset carAsset) {
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

bool CarQuery::CanStripBumpers(AmigaLibAsset carAsset) {
    switch (carAsset) {
        case FORD_DEUCE_COUPE_1932_BODY:
        case FORD_SEDAN_1932_BODY:
        case THE_KING_CHEVROLET_CORVETTE_1963_BODY:
        // FIXME: These SHOULD be able to strip bumpers, the bitmaps are garbled
        case CHEVROLET_CORVETTE_1956_BODY:
        case CHEVROLET_CORVETTE_1961_BODY:
        case CHEVROLET_CORVETTE_1963_BODY:
            return false;
        default:
            return true;
    }
}

int CarQuery::GetChoppedBumberRearIndex(AmigaLibAsset carAsset) {
    switch (carAsset) {
        case CHEVROLET_CORVETTE_1956_BODY:
        case CHEVROLET_CORVETTE_1961_BODY:
        case CHEVROLET_CORVETTE_1963_BODY:
            return false;
        default:
            return true;
    }

    return carAsset + 2;
}

int CarQuery::GetChoppedBumberFrontIndex(AmigaLibAsset carAsset) {
    return carAsset + 3;
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
            checksum = ((checksum << 5) + checksum) + buffer[i]; // Simple rolling hash
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

    printf("File signature verified successfully\n");
    return SrLibStatusSuccess;
}