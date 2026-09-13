#include "ui/main_font.h"

#include "core/log.h"

SFont_Font* LoadMainFontFromExe(const std::vector<uint8_t>& srExeData) {
    // Amiga embedded font offsets in the Street_Rod binary.
    // See RodLegacyResearch/docs/formats/amiga_sr_font.md for derivation.
    constexpr size_t   BITMAP_OFFSET    = 0x0295A0;
    constexpr size_t   CHARLOC_OFFSET   = 0x029740;
    constexpr size_t   CHARSPACE_OFFSET = 0x0298B0;
    constexpr int      YSIZE            = 8;
    constexpr int      MODULO           = 52;
    constexpr int      LO_CHAR          = 0x20;
    constexpr int      HI_CHAR          = 0x7A;
    constexpr int      NUM_GLYPHS       = HI_CHAR - LO_CHAR + 1; // 91

    const size_t min_size = CHARSPACE_OFFSET + NUM_GLYPHS * 2;
    if (srExeData.size() < min_size) {
        LOG_INFO("Amiga exe too small for embedded font (have %zu, need %zu), keeping embedded font",
                 srExeData.size(), min_size);
        return nullptr;
    }

    // Unpack the 1-bit strip into a byte-per-pixel array (1=set, 0=clear).
    // Bitmap layout: MODULO bytes per row, YSIZE rows; MSB = leftmost pixel.
    const int STRIP_W = MODULO * 8; // 416 pixel columns
    std::vector<uint8_t> unpacked(YSIZE * STRIP_W);
    const uint8_t* bitmap = srExeData.data() + BITMAP_OFFSET;
    for (int row = 0; row < YSIZE; row++) {
        for (int col = 0; col < STRIP_W; col++) {
            int byte_i = row * MODULO + col / 8;
            int bit_i  = 7 - (col % 8);
            unpacked[row * STRIP_W + col] = (bitmap[byte_i] >> bit_i) & 1;
        }
    }

    // Read CharLoc (big-endian UWORD pairs: bitoffset, bitwidth).
    // Read CharSpace (big-endian WORD advance widths).
    // Skip space (0x20); encode '!' (0x21) through 'z' (0x7A).
    constexpr int FIRST_CHAR = 0x21;
    constexpr int NUM_CHARS  = HI_CHAR - FIRST_CHAR + 1; // 90

    const uint8_t* cl = srExeData.data() + CHARLOC_OFFSET;
    const uint8_t* cs = srExeData.data() + CHARSPACE_OFFSET;

    std::vector<int> x_offsets(NUM_CHARS), glyph_widths(NUM_CHARS), advance_widths(NUM_CHARS);
    for (int i = 0; i < NUM_CHARS; i++) {
        int char_idx = (FIRST_CHAR + i) - LO_CHAR;
        int cl_off   = char_idx * 4;
        x_offsets[i]      = ((uint16_t)cl[cl_off + 0] << 8) | cl[cl_off + 1];
        glyph_widths[i]   = ((uint16_t)cl[cl_off + 2] << 8) | cl[cl_off + 3];
        advance_widths[i] = (int16_t)(((uint16_t)cs[char_idx * 2] << 8) | cs[char_idx * 2 + 1]);
    }

    SFont_Font* amigaFont = SFont_InitFontDirect(
        unpacked.data(), STRIP_W,
        x_offsets.data(), glyph_widths.data(), advance_widths.data(),
        FIRST_CHAR, NUM_CHARS, YSIZE,
        255, 255, 255
    );

    if (!amigaFont) {
        LOG_INFO("SFont_InitFontDirect failed for Amiga font, keeping embedded font");
        return nullptr;
    }

    return amigaFont;
}
