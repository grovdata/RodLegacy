#include "pc_speaker.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <string>

int main(int argc, char* argv[])
{
    std::string exe_path = "SR_UNP.EXE";
    int seg = 0;

    if (argc > 1) exe_path = argv[1];
    if (argc > 2) seg = std::atoi(argv[2]);

    SDL_Init(SDL_INIT_AUDIO);

    std::vector<std::vector<SrNote>> segments;
    if (!sr_exe_parse_music(exe_path, segments)) {
        fprintf(stderr, "Failed to parse %s\n", exe_path.c_str());
        return 1;
    }
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
