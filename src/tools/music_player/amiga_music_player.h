#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct AdfFileData;

namespace sr {

struct InstrumentDef {
    uint16_t envelope_index = 0;
    uint16_t unknown = 0;
    uint16_t bank_index = 0;
    uint16_t sample_length_words = 0;
    uint16_t sample_offset_bytes = 0;
};

struct EnvelopeDef {
    int16_t points[16] = {};
};

struct SongData {
    std::vector<uint8_t> note_bytes;
    std::vector<uint8_t> ins_bytes;
    std::vector<uint8_t> env_bytes;
    std::vector<int8_t> wave0_bytes;
    std::vector<int8_t> wave1_bytes;
};

bool LoadSongDataFromDirectory(const std::string& directory, SongData& out_data, std::string& out_error);
bool LoadSongDataFromAdf(const std::string& adf_path, SongData& out_data, std::string& out_error);

int RunMusicPlayer(const SongData& song, bool loop_forever, bool debug, int debug_events, int base_transpose, uint32_t muted, int scan_irqs,
                   const std::vector<std::pair<int,int>>& bank_overrides, int start_pattern);

}  // namespace sr
