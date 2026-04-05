#include "pc_speaker.h"
#include <exepack/unpack.hpp>
#include <SDL2/SDL.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    std::string exe_path = "SR.EXE";
    int seg = 0;

    if (argc > 1) exe_path = argv[1];
    if (argc > 2) seg = std::atoi(argv[2]);

    std::ifstream f(exe_path, std::ios::binary | std::ios::ate);
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", exe_path.c_str());
        return 1;
    }
    auto packed_size = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> packed(packed_size);
    if (!f.read(reinterpret_cast<char*>(packed.data()), static_cast<std::streamsize>(packed_size))) {
        fprintf(stderr, "Failed to read %s\n", exe_path.c_str());
        return 1;
    }

    exepack::UnpackExeResult unpacked = exepack::unpack_exepack_to_exe_bytes(packed.data(), packed.size());
    if (!unpacked.ok()) {
        fprintf(stderr, "EXEPACK unpack failed: %s\n", unpacked.error.c_str());
        return 1;
    }

    std::vector<std::vector<SrNote>> segments;
    if (!sr_exe_parse_music_bytes(unpacked.exe_bytes.data(), unpacked.exe_bytes.size(), segments)) {
        fprintf(stderr, "Failed to parse unpacked SR.EXE music data\n");
        return 1;
    }

    SDL_Init(SDL_INIT_AUDIO);

    fprintf(stderr, "Parsed %zu segments\n", segments.size());
    for (int i = 0; i < (int)segments.size(); i++)
        fprintf(stderr, "  segment[%2d]: %zu notes\n", i, segments[i].size());

    if (seg >= (int)segments.size()) seg = 0;

    PCSpeaker speaker;
    if (!speaker.Init()) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return 1;
    }
    speaker.SetNotes(segments[seg]);
    speaker.Play();
    fprintf(stderr, "Playing segment %d. Press Enter to advance, q+Enter to quit.\n", seg);

    while (true) {
        int c = fgetc(stdin);
        if (c == 'q' || c == EOF) break;
        seg = (seg + 1) % (int)segments.size();
        fprintf(stderr, "-> segment %d (%zu notes)\n", seg, segments[seg].size());
        speaker.SetNotes(segments[seg]);
        speaker.Play();
    }

    speaker.Stop();
    speaker.Shutdown();
    SDL_Quit();
    return 0;
}
