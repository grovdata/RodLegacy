#include <iostream>
#include <vector>


struct Sprite {
    int width;
    int height;
    int depth;
    std::vectoruint8_t> rle_dictionary;
};

std::vectoruint8_t> decode_rle(const std::vectorpair<int, uint8_t>>& rle, const Sprite& sprite, const std::vectoruint8_t>& data) {
    int p = 0;
    int width = sprite.width / 8;
    int height = sprite.height;
    int depth = sprite.depth;
    int size_expected = (sprite.width * sprite.height * sprite.depth) / 8;

    std::vectoruint8_t> flat_bp;

    int pos = 0;
    cout << "total " << (width * height * depth) << endl;

    while (pos < width * height * depth) {
        uint8_t b = data[p];

        bool found = false;
        const std::vectoruint8_t>& directory = sprite.rle_dictionary;

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
            for (int _ = 0; _ < c + 1; ++_) {
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

int main() {
    // Example usage
    std::std::vectorstd::pair<int, uint8_t>> rle = { {1, 0x0}, {1, 0xff}, {2, 0x0}, {3, 0x0}, {2, 0xff}, {4, 0x0}, {5, 0x0}, {3, 0xff}, {6, 0x0}, {4, 0xff}, {7, 0x0}, {5, 0xff}, {8, 0x0}, {6, 0xff}, {9, 0x0}, {7, 0xff} };
    Sprite sprite = { 64, 64, 1, {0, 0xff, 0x0, 0x4e, 0x3, 0x0, 0x3, 0x2, 0x0, 0x0, 0x3, 0x1, 0x0, 0xff, 0, 0} };
    std::std::vectoruint8_t> data = { /* your data here */ };

    std::std::vectoruint8_t> decoded_data = decode_rle(rle, sprite, data);

    for (uint8_t byte : decoded_data) {
        std::cout << (int)byte << " ";
    }

    return 0;
}
