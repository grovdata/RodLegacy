#include "audio/pc_speaker.h"
#include <exepack/unpack.hpp>
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <termios.h>
#include <unistd.h>

static constexpr float ISR_RATE_HZ = 1193182.0f / 16384.0f;

static float SegDurSec(const std::vector<SrNote>& seg) {
    uint32_t ticks = 0;
    for (const auto& n : seg) ticks += n.duration_ticks;
    return (float)ticks / ISR_RATE_HZ;
}

static std::vector<uint8_t> LoadAndUnpack(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)sz);

    exepack::UnpackExeResult res = exepack::unpack_exepack_to_exe_bytes(buf.data(), buf.size());
    if (res.ok()) return res.exe_bytes;
    return buf;
}

static struct termios s_saved_termios;
static bool s_raw_mode = false;

static void EnableRawMode() {
    tcgetattr(STDIN_FILENO, &s_saved_termios);
    struct termios raw = s_saved_termios;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;   // non-blocking read
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    s_raw_mode = true;
}

static void DisableRawMode() {
    if (s_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &s_saved_termios);
        s_raw_mode = false;
    }
}

static char PollKey() {
    char c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

static void Usage(const char* prog) {
    printf("Usage: %s [options] SR.EXE\n\n", prog);
    printf("Options:\n");
    printf("  (default)  known songs in sequence (5 songs), Enter=next\n");
    printf("  --shuffle  same songs in shuffled order\n");
    printf("  --segs     play raw CODE_24 segments one at a time\n");
    printf("  --songs    play playlist-assembled song blocks\n");
    printf("  --seg N    start at segment N (--segs mode)\n");
    printf("\nControls: Space=mark  Enter=next  q=quit\n");
}

int main(int argc, char* argv[]) {
    const char* exe_path = "SR.EXE";
    bool mode_segs  = false;
    bool mode_songs = false;
    int  start_seg  = 0;

    bool mode_shuffle = false;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--shuffle") == 0) mode_shuffle = true;
        else if (strcmp(argv[i], "--segs")    == 0) mode_segs    = true;
        else if (strcmp(argv[i], "--songs")   == 0) mode_songs   = true;
        else if (strcmp(argv[i], "--seg") == 0 && i + 1 < argc)
            start_seg = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            Usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            exe_path = argv[i];
        }
    }
    if (!mode_shuffle && !mode_segs && !mode_songs) mode_shuffle = true;

    std::vector<uint8_t> exe = LoadAndUnpack(exe_path);
    if (exe.empty()) {
        fprintf(stderr, "Cannot open: %s\n", exe_path);
        return 1;
    }

    std::vector<std::vector<SrNote>> segs;
    if (!sr_exe_parse_music_bytes(exe.data(), exe.size(), segs)) {
        fprintf(stderr, "Failed to parse SR.EXE music data\n");
        return 1;
    }

    std::vector<std::vector<SrNote>> songs;
    std::vector<SrNote> bridge;
    bool has_songs = sr_exe_parse_songs(exe.data(), exe.size(), songs, bridge);

    printf("=== CODE_24 raw segments: %zu ===\n", segs.size());
    for (int i = 0; i < (int)segs.size(); i++)
        printf("  seg[%2d]  %4zu notes  %6.2fs\n", i, segs[i].size(), SegDurSec(segs[i]));

    if (has_songs) {
        printf("\n=== Playlist songs: %zu ===\n", songs.size());
        printf("  bridge   %4zu notes  %6.2fs\n", bridge.size(), SegDurSec(bridge));
        for (int i = 0; i < (int)songs.size(); i++) {
            printf("  song[%d]  %4zu notes  %6.2fs  (with bridge: %.2fs)\n",
                   i, songs[i].size(), SegDurSec(songs[i]),
                   SegDurSec(bridge) + SegDurSec(songs[i]));
        }
    } else {
        printf("\n(playlist structure not found -- using --segs mode)\n");
        mode_songs = false;
        mode_segs  = true;
    }
    printf("\nControls: Space=mark  Enter=next  q=quit\n\n");

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    PCSpeaker spk;
    if (!spk.Init()) {
        fprintf(stderr, "PCSpeaker init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    struct Item {
        std::vector<SrNote> notes;
        float dur;
        char  label[64];
    };
    std::vector<Item> items;

    if (mode_shuffle || (!mode_segs && !mode_songs)) {
        std::vector<std::vector<SrNote>> known_songs;
        if (!sr_exe_build_known_songs(segs, known_songs)) {
            fprintf(stderr, "sr_exe_build_known_songs failed: expected >=18 segments, got %zu\n",
                    segs.size());
            SDL_Quit(); return 1;
        }

        static const char* kNames[] = {"song1", "song2", "song3", "song4", "song5"};

        std::vector<int> order;
        for (int i = 0; i < (int)known_songs.size(); i++) order.push_back(i);
        if (mode_shuffle)
            std::shuffle(order.begin(), order.end(), std::mt19937(std::random_device{}()));

        printf("\n=== %s mode: %zu songs ===\n",
               mode_shuffle ? "Shuffle" : "Sequence", known_songs.size());
        for (int i : order)
            printf("  %-8s  %.2fs\n", kNames[i], SegDurSec(known_songs[i]));

        for (int i : order) {
            Item it;
            it.notes = known_songs[i];
            it.dur   = SegDurSec(it.notes);
            snprintf(it.label, sizeof(it.label), "%s  %.2fs", kNames[i], it.dur);
            items.push_back(std::move(it));
        }
    } else if (mode_songs) {
        for (int i = 0; i < (int)songs.size(); i++) {
            Item it;
            it.notes = bridge;
            it.notes.insert(it.notes.end(), songs[i].begin(), songs[i].end());
            it.dur = SegDurSec(it.notes);
            snprintf(it.label, sizeof(it.label),
                     "song[%d/%d]  bridge=%.2fs + song=%.2fs = %.2fs",
                     i, (int)songs.size() - 1, SegDurSec(bridge), SegDurSec(songs[i]), it.dur);
            items.push_back(std::move(it));
        }
    } else {
        for (int i = 0; i < (int)segs.size(); i++) {
            Item it;
            it.notes = segs[i];
            it.dur   = SegDurSec(segs[i]);
            snprintf(it.label, sizeof(it.label),
                     "seg[%2d/%d]  %zu notes  %.2fs",
                     i, (int)segs.size() - 1, segs[i].size(), it.dur);
            items.push_back(std::move(it));
        }
    }

    int idx = (mode_segs && start_seg < (int)items.size()) ? start_seg : 0;

    auto StartItem = [&](int i) {
        spk.SetNotes(items[i].notes);
        spk.Play();
        printf("\n%s\n", items[i].label);
    };

    StartItem(idx);
    uint64_t t0 = SDL_GetTicks64();

    EnableRawMode();

    bool running = true;
    while (running) {
        SDL_Delay(50);

        float elapsed = (SDL_GetTicks64() - t0) / 1000.0f;
        float dur     = items[idx].dur;
        float pos     = elapsed;
        int   loop    = 0;
        if (dur > 0.0f) {
            loop = (int)(elapsed / dur);
            pos  = elapsed - loop * dur;
        }
        printf("\r  [loop %d]  %.1f / %.1fs    ", loop, pos, dur);
        fflush(stdout);

        if (spk.ConsumeLoopFlag()) {
            idx = (idx + 1) % (int)items.size();
            StartItem(idx);
            t0 = SDL_GetTicks64();
        }

        char c = PollKey();
        if (c == 0) continue;

        if (c == 'q') {
            running = false;
        } else if (c == ' ') {
            printf("\r  [MARK] loop=%d  pos=%.2fs / %.2fs  abs=%.2fs\n",
                   loop, pos, dur, elapsed);
        } else if (c == '\r' || c == '\n') {
            idx = (idx + 1) % (int)items.size();
            StartItem(idx);
            t0 = SDL_GetTicks64();
        }
    }

    DisableRawMode();
    printf("\n");

    spk.Stop();
    spk.Shutdown();
    SDL_Quit();
    return 0;
}
