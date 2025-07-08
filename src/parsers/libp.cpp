#include "libp.h"



#include <fstream>
#include <iostream>
#include <sstream>

#include "carcolor.h"

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

/// @brief Prints bitplanes in text format
/// @param data
/// @param width
/// @param height
/// @param depth
void SrLibDebugBitplanes(const uint8_t* data, int width, int height, int depth) {
    const int bytesPerColumn = 4;
    const int maxColumns = 8;
    const int totalBytes = width * height * depth;

    for (int i = 0; i < totalBytes; i++) {
        // Print each byte as a two-digit hexadecimal number with padding
        printf("%02x", data[i]);

        // Insert a space after every bytesPerColumn bytes
        if ((i + 1) % bytesPerColumn == 0) {
            printf(" ");
        }

        // Insert a newline after every maxColumns * bytesPerColumn bytes
        if ((i + 1) % (bytesPerColumn * maxColumns) == 0) {
            printf("\n");
        }
    }

    // Handle the case where the last row may not be completely filled
    if (totalBytes % (bytesPerColumn * maxColumns) != 0) {
        printf("\n");
    }
}


/// @brief Read metadata about bitmaps in library file
/// @param filename
/// @return Map of int (total entries) and BitmapEntry_t metadata struct
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
        
        SrLibDebugBitmapEntry(i, newEntry);
        result.push_back(newEntry);
    }
    
    return std::pair<int, std::vector<BitmapEntry_t>>(totalEntriesCount, result);
}

std::vector<uint8_t> BitmapParser::GetPackedData(const std::vector<uint8_t>& data, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    int headerSize = 2 + (sizeof(BitmapEntry_t) * totalEntriesCount);
    int fileOffset = headerSize + bitmapEntry.offset;
    
    if (fileOffset + bitmapEntry.sizePacked > (int)data.size()) {
        throw std::runtime_error("Can't read data because offset is larger than data size.");
    }
    
    return std::vector<uint8_t>(
        data.begin() + fileOffset,
        data.begin() + fileOffset + bitmapEntry.sizePacked
    );
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

std::vector<SrLibColor> BitmapParser::PlanarToChunky(const std::vector<uint8_t>& data, 
    BitmapEntry_t entry, const Palette& palette, const Palette& carPalette) {
    std::vector<SrLibColor> chunkyData(entry.width * entry.height);

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

                // Determine which palette to use
                bool useLowerRange = value >= 1 && value <= 9;
                bool useUpperRange = value >= 33 && value <= 41;
                bool useCarPalette = !carPalette.empty() && (useLowerRange || useUpperRange);
                const Palette& selectedPalette = useCarPalette ? carPalette : palette;
                
                // Ensure value is within bounds of the selected palette
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

// Converts 12bit palette to RGB and adds EHB colors needed for the Amiga graphics
Palette BitmapParser::PreparePalette(const std::vector<uint16_t>& palette, bool ehb) {
    Palette rval;

    // Add RGB colors from the palette
    for (int i = 0; i < int(palette.size()); i++) {
        rval.push_back(GetRGBFromRGB4(palette[i]));
    }

    if (ehb) {
        // Add EHB colors
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

// Creates a black palette except for color 1-10 (car shades) and EHB counterparts
Palette BitmapParser::PrepareCarPalette(const std::vector<uint16_t>& palette) {
    Palette rval;

    // Add RGB colors from the palette
    for (int i = 0; i < 32; i++) {
        rval.push_back({0, 0, 0, 255});
    }

    for (int i = 0; i < 9; i++) {
        rval.at(i + 1) = GetRGBFromRGB4(palette[i]);
    }

    // Add EHB colors
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
            image[pos + 0] = c.r;    // R
            image[pos + 1] = c.g;    // G
            image[pos + 2] = c.b;    // B
            image[pos + 3] = c.a;    // A
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

    // for (int i = 0; i < entries.second.size(); i++) {
    //     auto b = entries.second[i];
    //     vector<uint8_t> imgData = ReadImage(filename, b, i, entries.first);
    //     result.push_back(make_pair(b, imgData));
    // }
    // Palette pal = p.PreparePalette(PAL1);
    // vector<Color> chunkyData = p.PlanarToChunky(data, b, pal);

    return result;
}

std::vector<uint8_t> BitmapParser::ReadImage(const std::vector<uint8_t>& data, BitmapEntry_t entry, int, int totalEntriesCount) {
    std::vector<uint8_t> packedData = GetPackedData(data, entry, totalEntriesCount);
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
std::pair<int, int> BitmapParser::GetSpritesheetInfo(SrLibFile location, int index) {
    switch (location) {
        case SrLibFileAmigaLibP: {
            AmigaLibAsset a = (AmigaLibAsset)index;
            switch (a) {
                case CAR_TIRES_BRAND:
                case CAR_TIRES_RACING:
                case CAR_TIRES_STOCK:
                    return std::make_pair(1, 4);
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
        default:
            return PAL1;
    }
}

std::vector<uint16_t> BitmapParser::GetPalette(SrLibFile location, int index) {
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
std::pair<int, std::vector<BitmapEntry_t>> BitmapParser::ReadEntries(const std::string& filename) {
    // Read file into vector
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::make_pair(0, std::vector<BitmapEntry_t>());
    }
    
    // Get file size and create vector
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
       std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::make_pair(0, std::vector<BitmapEntry_t>());
    }
    
    // Use the new vector-based implementation
    return ReadEntries(buffer);
}

// File-based wrapper for GetPackedData
std::vector<uint8_t> BitmapParser::GetPackedData(const std::string& filename, const BitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    // Read file into vector
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::vector<uint8_t>();
    }
    
    // Get file size and create vector
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::vector<uint8_t>();
    }
    
    // Use the new vector-based implementation
    return GetPackedData(buffer, bitmapEntry, totalEntriesCount);
}

// File-based wrapper for ReadImage
std::vector<uint8_t> BitmapParser::ReadImage(const std::string& filename, BitmapEntry_t entry, int index, int totalEntriesCount) {
    // Read file into vector
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "File error: Unable to open file " << filename << std::endl;
        return std::vector<uint8_t>();
    }
    
    // Get file size and create vector
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    
    // Read file content into vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "File error: Unable to read file " << filename << std::endl;
        return std::vector<uint8_t>();
    }
    
    // Use the new vector-based implementation
    return ReadImage(buffer, entry, index, totalEntriesCount);
}



void SrLibDataCleanupCarString(char* str) {
    char* src;
    char* dst;
    int len = strlen(str);

    // Replace double backticks (``) with a space
    src = dst = str;
    while (*src) {
        if (src[0] == '`' && src[1] == '`') {
            *dst++ = ' ';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    // Replace single backticks (`) with a space
    src = dst = str;
    while (*src) {
        if (*src == '`') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    // Remove trailing period if present
    len = strlen(str);
    if (len > 0 && str[len - 1] == '.') {
        str[len - 1] = '\0';
    }
}

// 0xA849C
std::vector<std::string> SrLibReadStrings(const uint8_t* data, int offset, size_t dataSize) {
    std::vector<std::string> result;
    const char* ptr = (const char*)data + offset;
    while (ptr < (const char*)data + dataSize) {
        result.push_back(std::string(ptr));
        ptr += strlen(ptr) + 1;
    }
    return result;
}

/// @brief Reads info about cars and parts from data file (SR_data)
/// @param data Pointer to data buffer
/// @param dataSize Size of data buffer
/// @param outCount Pointer to store number of entries
/// @return Array of CarMetadata_t entries (must be freed by caller)
CarMetadata_t* DataParser::SrLibDataReadDataFromBuffer(const uint8_t* data, size_t dataSize, int* outCount) {
    const int OFFSETS_START = 0x34C;
    const int CLICKBOX_START = 0x484; // ????
    const int CLICKBOX_GASSTATION_START = 0x68c;
    const int CLASSIFIEDS_START = 0x854;

    if (!data || !outCount || dataSize < OFFSETS_START) return NULL;
    
    *outCount = CAR_PLAYER_MODEL_COUNT;
    CarMetadata_t* result = (CarMetadata_t*)calloc(CAR_PLAYER_MODEL_COUNT, sizeof(CarMetadata_t));
    if (!result) return NULL;

    const uint8_t* ptr = data + OFFSETS_START;

    // Read offsets
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
        
        // Add supplemental offsets
        StockCarModels model = (StockCarModels)i;
        newEntry.offsetYTires = CAR_WHEELS_Y_OFFSETS[model][1];
        newEntry.offsetXGasCap = CAR_GAS_CAP_XY_OFFSETS[model][1];
        newEntry.offsetYGasCap = CAR_GAS_CAP_XY_OFFSETS[model][2];

        result[i].offsets = newEntry;
    }

    // Parse Classifieds
    ptr = data + CLASSIFIEDS_START;
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        CarMetadata_t* metadataEntry = &result[i];
        ClassifiedEntry_t newClassified = {};  // Zero initialize

        // Read null-terminated string
        const char* carNameStr = (const char*)ptr;
        ptr += strlen(carNameStr) + 1;  // Skip past null terminator

        // Parse year
        char yearStr[5] = {0};
        strncpy(yearStr, carNameStr, 4);
        newClassified.year = atoi(yearStr);
        
        // Allocate and copy car name
        char* carName = (char*)malloc(strlen(carNameStr) + 1);
        if (!carName) goto cleanup;
        strcpy(carName, carNameStr);
        SrLibDataCleanupCarString(carName);
        newClassified.carName = carName;

        // Read and process ad text
        char* adText = (char*)malloc(1024);
        if (!adText) {
            free(carName);
            goto cleanup;
        }
        adText[0] = '\0';
        
        while (true) {
            const char* lineStr = (const char*)ptr;
            ptr += strlen(lineStr) + 1;  // Skip past null terminator

            char* line = (char*)malloc(strlen(lineStr) + 1);
            if (!line) {
                free(carName);
                free(adText);
                goto cleanup;
            }
            strcpy(line, lineStr);
            SrLibDataCleanupCarString(line);

            const char* dollarPos = strchr(line, '$');
            if (!lineStr[0] || dollarPos) {
                // Found price or empty line
                if (dollarPos) {
                    newClassified.price = atoi(dollarPos + 1);
                }
                free(line);
                break;
            }

            // Append line to ad text
            if (adText[0]) {
                strcat(adText, " ");
            }
            strcat(adText, line);
            free(line);
        }
        newClassified.adText = adText;
        metadataEntry->classified = newClassified;
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
    // If we hit an error, clean up what we've allocated so far
    for (int i = 0; i < CAR_PLAYER_MODEL_COUNT; i++) {
        SrLibDataFreeClassified(&result[i].classified);
    }
    free(result);
    *outCount = 0;
    return NULL;
}

// Convert Amiga sprite data to RGBA buffer
// data: Pointer to sprite data
// palette: Array of Amiga format colors (0000 rrrrggggbbbb)
// sprite_num: Sprite number (0-7) to determine color registers
// width: Sprite width (typically 16)
// height: Sprite height
// out_buffer: Pre-allocated RGBA buffer of size width*height
int SrLibAmigaSpriteToRGBA(const uint16_t* data,
                           int startOffset,
                           const uint16_t* palette,
                           int sprite_num,
                           int width, 
                           int height,
                           SrLibColor* out_buffer) {

    
    const uint8_t* byteData = reinterpret_cast<const uint8_t*>(data) + startOffset;
    
    // Debug: Print sprite info
    printf("Sprite %d (%dx%d)\n", sprite_num, width, height);
    printf("Color register base: %d\n", 16 + ((sprite_num & 6) * 2) + 1);
    
    // Process each line
    for (int y = 0; y < height; y++) {
        // Each line has 2 words: DATA and DATB
        // Read big-endian 16-bit values manually
        uint16_t data_word = (byteData[0] << 8) | byteData[1];
        uint16_t datb_word = (byteData[2] << 8) | byteData[3];
        byteData += 4;  // Advance to next line
        
        printf("Line %2d: [%04X %04X] ", y, data_word, datb_word);
        
        // Process each pixel in the line
        for (int x = 0; x < width; x++) {
            // Extract bits from most significant to least significant
            int bit_pos = 15 - x;
            int data_bit = (data_word >> bit_pos) & 1;
            int datb_bit = (datb_word >> bit_pos) & 1;
            int color_idx = (datb_bit << 1) | data_bit;  // Combine bits to get 0-3 value
            
            printf("%d", color_idx);  // Debug: print color index
            
            SrLibColor* pixel = &out_buffer[y * width + x];
            if (color_idx == 0) {
                // Color 0 is transparent
                pixel->r = pixel->g = pixel->b = pixel->a = 0;
            } else {
                // Get color from sprite palette registers
                // Sprites are paired (0/1, 2/3, etc) and share color registers
                uint16_t palette_color = palette[17 + ((sprite_num & 6) * 2) + color_idx - 1];
                SrLibColor rgb_color = BitmapParser::GetRGBFromRGB4(palette_color);
                pixel->r = rgb_color.r;
                pixel->g = rgb_color.g;
                pixel->b = rgb_color.b;
                pixel->a = 255;
            }
        }
        printf("\n");
    }

    return 0;
}

int SrLibAmigaSpriteReadMousePointer(const uint16_t* data, SrLibAmigaSpriteAsset sprite, SrLibColor* out_buffer) {
    uint16_t pal[] = PAL1;
    SrLibAmigaSpriteInfo info = SPRITE_INFO_LOOKUP[sprite];
    return SrLibAmigaSpriteToRGBA(data, info.offset, pal, int(sprite), info.width, info.height, out_buffer);    
}

// File-based wrapper
CarMetadata_t* DataParser::SrLibDataReadDataFromFile(const char* filename, int* outCount) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "File error: Unable to open file %s\n", filename);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file into buffer
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

    // Process the data
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
    int stockCarCount = sizeof(CAR_INDEX);

    for (int i = 0; i < stockCarCount; i++) {
        if (CAR_INDEX[i] == (int)asset) {
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

int SrLibQueryGetCarRoofYOffset(SrLibGameScene scene, int carIndex) {
    for (size_t i = 0; i < CAR_Y_OFFSETS_COUNT; i++) {
        if (CAR_Y_OFFSETS[i][0] == carIndex) {
            // Add 1 to scene index since car model is in column 0
            return CAR_Y_OFFSETS[i][(int)(scene) + 1];
        }
    }
    return 96; // Default offset if car not found
}

// Helper function to get car X position for a specific scene
int SrLibQueryGetCarIdleX(SrLibGameScene scene) {
    for (size_t i = 0; i < CAR_IDLE_X_COUNT; i++) {
        if (CAR_IDLE_X[i][0] == scene) {
            return CAR_IDLE_X[i][1];
        }
    }
    return 35; // Default X position if scene not found
}


int CarQuery::SrLibQueryGetChoppedBumberRearIndex(AmigaLibAsset carAsset) {
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

int CarQuery::SrLibQueryGetChoppedBumberFrontIndex(AmigaLibAsset carAsset) {
    return carAsset + 3;
}

CarVendor_t SrLibQueryGetCarVendor(int carIndex) {
     for (size_t i = 0; i < CAR_INDEX_COUNT; i++) {
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