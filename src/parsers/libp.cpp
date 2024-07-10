#include "libp.h"
using namespace std;

int8_t BitmapParser::readUint8(uint8_t *result, FILE *f) {
    return fread(result, sizeof(uint8_t), 1, f);
}
int8_t BitmapParser::readUint16BigEndian(uint16_t *result, FILE *f) {
    uint8_t buffer[sizeof(int16_t)];
    if (!result || !f || sizeof(uint16_t) != fread((void *)buffer, 1, sizeof(uint16_t), f)) return 0;
    *result = (buffer[0] << 8) + buffer[1];
    return 1;
}
int8_t BitmapParser::readUint32BigEndian(uint32_t *result, FILE *f) {
    uint8_t buffer[sizeof(int32_t)];
    if (!result || !f || sizeof(uint32_t) != fread((void *)buffer, 1, sizeof(uint32_t), f)) return 0;
    *result = (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
    return 1;
}
void BitmapParser::printEntry(int number, LibBitmapEntry_t *entry) {
    printf("---------\n");
    printf("bitmap %d\n", number);
    printf("width %d\n", entry->width);
    printf("height %d\n", entry->height);
    printf("depth %d\n", entry->depth);
    printf("unknown1 %d\n", entry->unknown1);
    printf("offset %d\n", entry->offset);
    printf("sizePacked %d\n", entry->sizePacked);
}

void BitmapParser::printBitplanes(uint8_t *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
    }
}

std::vector<LibBitmapEntry_t> BitmapParser::GetEntries(char *filename) {
    std::vector<LibBitmapEntry_t> result;
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        fputs("File error", stderr);
        return result;
    }

    uint16_t totalEntriesCount = 0;

    if (readUint16BigEndian(&totalEntriesCount, fp) == 1) {
        printf("Number of bitmap entries in %s: %d\n", filename, totalEntriesCount);

        for (int i = 0; i < totalEntriesCount; i++) {
            LibBitmapEntry_t newEntry = {0};
            readUint16BigEndian(&newEntry.width, fp);
            readUint16BigEndian(&newEntry.height, fp);
            readUint8(&newEntry.depth, fp);
            readUint8(&newEntry.unknown1, fp);
            fread(&newEntry.rleDictionary, 16, 1, fp);
            readUint32BigEndian(&newEntry.offset, fp);
            readUint16BigEndian(&newEntry.sizePacked, fp);

            printEntry(i, &newEntry);
            result.push_back(newEntry);
        }
    }

    fclose(fp);
    return result;
}

bool BitmapParser::GetPackedData(char *filename, uint8_t *buf, LibBitmapEntry_t *bitmapEntry, int totalEntriesCount) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        fputs("File error", stderr);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);

    int fileOffset = 2 + (sizeof(LibBitmapEntry_t) * totalEntriesCount) + bitmapEntry->offset;

    if (fileOffset >= fileSize) {
        printf("Can't read data because offset %d is larger than file size %d.\n", fileOffset, fileSize);
        return false;
    }

    fseek(fp, fileOffset, SEEK_SET);
    fread(buf, bitmapEntry->sizePacked, 1, fp);
    fclose(fp);
    return true;
}

vector<uint8_t> BitmapParser::UnpackData(const vector<pair<int, uint8_t>> &rle, const LibBitmapEntry_t &sprite, const vector<uint8_t> &data) {
    int p = 0;
    int width = sprite.width / 8;
    int height = sprite.height;
    int depth = sprite.depth;
    int size_expected = (sprite.width * sprite.height * sprite.depth) / 8;

    vector<uint8_t> flat_bp;

    int pos = 0;

    while (pos < width * height * depth) {
        uint8_t b = data[p];

        bool found = false;
        const uint8_t *directory = sprite.rleDictionary;

        for (int i = 0; i < 16; ++i) {
            if (directory[i] == 0) {
                break;
            }
            if (b == directory[i]) {
                for (int c = 0; c < rle[i].first; ++c) {
                    try {
                        ++pos;
                        flat_bp.push_back(rle[i].second);
                    } catch (...) {
                        continue;
                    }
                }
                found = true;
                break;
            }
        }

        if (found) {
            ++p;
            continue;
        }

        if (b == 0 || b == 0xFF) {
            uint8_t c = data[p + 1] & 0xFF;
            for (int i = 0; i < c + 1; ++i) {
                flat_bp.push_back(b);
                ++pos;
            }
            p += 2;
        } else {
            flat_bp.push_back(b);
            ++pos;
            ++p;
        }
    }

    if (flat_bp.size() < size_expected) {
        // Pad data at end
        int missing = size_expected - flat_bp.size();
        flat_bp.insert(flat_bp.end(), missing, 0x0);
    }

    return flat_bp;
}

std::vector<SDL_Surface *> BitmapParser::ReadAllImages(char *filename) {
    std::vector<SDL_Surface *> surfaces;

    // for (int i = 0; i < count; ++i) {
    // 	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, format);
    // 	if (!surface) {
    // 		std::cerr << "SDL_CreateRGBSurfaceWithFormat failed: " << SDL_GetError() << std::endl;
    // 	} else {
    // 		surfaces.push_back(surface);
    // 	}
    // }

    vector<LibBitmapEntry_t> entries = GetEntries(filename);

    return surfaces;
}

SDL_Surface *BitmapParser::ReadImage(char *filename, LibBitmapEntry_t entry, int index) {
    int expectedSize = (sprite.width * sprite.height * sprite.depth) / 8;
    uint8_t buf[expectedSize];
    GetPackedData(filename, &buf, &entry, );
}

void BitmapParser::OldUnpack(LibBitmapEntry_t *bitmapEntry, uint8_t *data, SDL_Surface *surface) {
    int p = 0;

    // int magicWord = data[p++] & 0xFF;
    //  if (magicWord != Sprite.IDENTIFIER) {
    //  	System.out.println("unknown compression");
    //  	return;
    //  }

    int width = bitmapEntry->width / 8;
    int height = bitmapEntry->height;

    int depth = 4;

    uint8_t bitmap[bitmapEntry->height * 5][bitmapEntry->height];
    // byte[][] bitmap = new byte[height * depth][width];

    int pos = 0;

    while (pos < width * height * depth) {
        uint8_t b = data[p++];

        bool found = false;

        for (int i = 0; i < 16; i++) {
            if (bitmapEntry->rleDictionary[i] == 0) {
                break;
            }
            if (b == bitmapEntry->rleDictionary[i]) {
                for (int c = 0; c < COUNT[i]; c++) {
                    bitmap[pos / width][pos % width] = VALUE[i];
                    pos++;
                }
                found = true;
                break;
            }
        }

        if (found) continue;

        if (b == 0) {
            int c = data[p++] & 0xFF;
            pos += c + 1;
        } else if (b == 0xFF) {
            int c = data[p++] & 0xFF;
            for (int i = 0; i <= c; i++) {
                bitmap[pos / width][pos % width] = 0xFF;
                pos++;
            }
        } else {
            bitmap[pos / width][pos % width] = b;
            pos++;
        }
    }

    // #define PAL_MONITOR_ID  0x00021000
    // #define LORES_KEY       0x00000000

    // typedef struct Color
    // {
    // 	UBYTE r, g, b;
    // }
    // Color;

    // struct
    // {
    // 	ULONG width;
    // 	ULONG height;
    // 	UWORD bitplaneDepth;
    // 	ULONG viewportMode;

    // 	Color color[32];
    // 	UBYTE *bitplanes[6];
    // }
    // viewport;

    // uint16_t palette[] = {0x0000, 0x0810, 0x0911, 0x0B21, 0x0D22, 0x0F32, 0x0F44, 0x0F65, 0x0F87, 0x0F98, 0x0FFF, 0x0EEE, 0x0CCC, 0x0BBB, 0x0AAA, 0x0999, 0x0888, 0x0950, 0x0C71, 0x0F96, 0x0333,
    // 0x0222, 0x005B, 0x007F, 0x00F0, 0x00D0, 0x0FE9, 0x0FD2, 0x0B21, 0x0F32, 0x009F, 0x0810}; amiVideo_Screen screen; amiVideo_initScreen(&screen, 320, 200, 5, 4, PAL_MONITOR_ID & LORES_KEY);
    // amiVideo_setBitplanePaletteColors(&palette, color, 32);
    // amiVideo_setScreenBitplanePointers(&screen, &bitmap);

    // // amiVideo_Screen screen;
    // // amiVideo_initScreen(&screen, 320, 200, 5, 4, 0);
    // // amiVideo_setBitplanePaletteColors(&palette, color, 32);

    // printf("Total image data size : %d\n" , bitmapEntry->width *height);

    // amiVideo_convertBitplaneColorsToChunkyFormat(palette);

    // surface =  SDL_CreateRGBSurface(0, screen.width, screen.height, 24, 0, 0, 0, 0);
    // amiVideo_setScreenUncorrectedRGBPixelsPointer(&screen, surface->pixels, surface->pitch, true, surface->format->Rshift, surface->format->Gshift, surface->format->Bshift,
    // surface->format->Ashift); if(SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0)
    // {
    // 	fprintf(stderr, "Cannot lock the surface!\n");
    // 	return;
    // }
    // amiVideo_convertScreenBitplanesToRGBPixels(&screen);

    // if(SDL_MUSTLOCK(surface))
    // 	SDL_UnlockSurface(surface);

    // construct 256-colors image
    int width2 = bitmapEntry->width;
    uint8_t bitmap2[height][width2];

    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            for (int i = 0; i < 8; i++) {
                // uint8_t to binary conversion
                // uint8_t 255 in bitmap becomes uint8_t 1,1,1,1,1,1,1,1 in bitmap2
                bitmap2[h][w * 8 + (7 - i)] = (uint8_t)((bitmap[h][w] & 1 << i) >> i);

                if (depth == 4) {
                    bitmap2[h][w * 8 + (7 - i)] |= (uint8_t)((bitmap[h + height][w] & 1 << i) >> i << 1 |      // 255 -> 2,2,2,2,2,2,2,2
                                                             (bitmap[h + height * 2][w] & 1 << i) >> i << 2 |  // 255 -> 4,4,4,4,4,4,4,4
                                                             (bitmap[h + height * 3][w] & 1 << i) >> i << 3 |  // 255 -> 8,8,8,8,8,8,8,8
                                                             (bitmap[h + height * 3][w] & 1 << i) >> i << 4);  //  | // 255 -> 8,8,8,8,8,8,8,8
                                                                                                               // (bitmap[h + height * 3][w] & 1 << i) >> i << 5) ; // 255 -> 8,8,8,8,8,8,8,8
                }
            }
        }
    }

    FILE *dump = fopen("/home/user/Code/openrod/src/rawdump.dat", "wb");
    printf("Image info: %dx%d\n", bitmapEntry->width, bitmapEntry->height);
    for (int h = 0; h < height; h++) {
        for (int w = 0; w < width; w++) {
            // uint8_t b = bitmap2[h][w];
            // int wrote = fwrite(&b, 1, 1, dump);
            // printf("writing %c %d\n",b,wrote);
        }
    }
    fclose(dump);

    // printf("bitmap2 %s\n",bitmap2);

    // target = SDL_CreateRGBSurfaceFrom((void*)bitmap2, width2, height, 8, width2, 0, 0, 0, 0);
}
