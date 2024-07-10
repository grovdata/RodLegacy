#include "libp.h"
#include <fstream>
#include <iostream>
using namespace std;

uint16_t BitmapParser::readUint16BigEndian(std::ifstream& file) {
    uint8_t bytes[2];
    file.read(reinterpret_cast<char*>(bytes), 2);
    return (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
}

uint32_t BitmapParser::readUint32BigEndian(std::ifstream& file) {
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

uint8_t BitmapParser::readUint8(std::ifstream& file) {
    uint8_t byte;
    file.read(reinterpret_cast<char*>(&byte), 1);
    return byte;
}

void BitmapParser::PrintEntry(int number, LibBitmapEntry_t entry) {
    printf("---------\n");
    printf("bitmap %d\n", number);
    printf("width %d\n", entry.width);
    printf("height %d\n", entry.height);
    printf("depth %d\n", entry.depth);
    printf("unknown1 %d\n", entry.unknown1);
    printf("offset %d\n", entry.offset);
    printf("sizePacked %d\n", entry.sizePacked);
}

void BitmapParser::PrintBitplanes(const std::vector<uint8_t>& data) {
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

vector<LibBitmapEntry_t> BitmapParser::GetEntries(const string& filename) {
    vector<LibBitmapEntry_t> result;
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "File error: Unable to open file " << filename << endl;
        return result;
    }

    uint16_t totalEntriesCount = readUint16BigEndian(file);
    cout << "Number of bitmap entries in " << filename << ": " << totalEntriesCount << endl;

    for (int i = 0; i < totalEntriesCount; ++i) {
        LibBitmapEntry_t newEntry = {};
        newEntry.width = readUint16BigEndian(file);
        newEntry.height = readUint16BigEndian(file);
        newEntry.depth = readUint8(file);
        newEntry.unknown1 = readUint8(file);
        file.read(reinterpret_cast<char*>(newEntry.rleDictionary), sizeof(newEntry.rleDictionary));
        newEntry.offset = readUint32BigEndian(file);
        newEntry.sizePacked = readUint16BigEndian(file);

        PrintEntry(i, newEntry);
        result.push_back(newEntry);
    }

    return result;
}

vector<uint8_t> BitmapParser::GetPackedData(const std::string& filename, const LibBitmapEntry_t& bitmapEntry, int totalEntriesCount) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("File error: Unable to open file");
    }

    file.seekg(0, ios::end);
    int fileSize = file.tellg();
    file.seekg(0, ios::beg);

    int fileOffset = 2 + (sizeof(LibBitmapEntry_t) * totalEntriesCount) + bitmapEntry.offset;

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

vector<pair<LibBitmapEntry_t, vector<uint8_t>>> BitmapParser::ReadAllImages(const string& filename) {
    vector<pair<LibBitmapEntry_t, vector<uint8_t>>> result;
    vector<LibBitmapEntry_t> entries = GetEntries(filename);

    return result;
}

vector<uint8_t> BitmapParser::ReadImage(const string& filename, LibBitmapEntry_t entry, int index) {
    int expectedSize = (entry.width * entry.height * entry.depth) / 8;
    vector<uint8_t> packedData = GetPackedData(filename, entry, 10);
    vector<uint8_t> data = UnpackData(RLE, entry, packedData);
    if (expectedSize != data.size()) {
        cout << "Expected size: " << expectedSize << " actual size: " << data.size() << endl;
        PrintBitplanes(data);
        throw runtime_error("Size mismatch reading bitmap");
    }
    return data;
}

int main() {
    string examplefile = "../../../streetrod-bins/amiga/SR_disk2/lib.p";
    BitmapParser p;
    vector<LibBitmapEntry_t> entries = p.GetEntries(examplefile.c_str());
    vector<uint8_t> data = p.ReadImage(examplefile.c_str(), entries[0], 0);
    // p.PrintBitplanes(data);
}