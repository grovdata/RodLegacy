// Street Rod 2 (DOS) AdLib music player - Nuked-OPL3 + SDL3.
//
//   --mus <file.MUS>  synthesise a song from MSR.SND + the .MUS event stream
//   --dro <file.dro>  replay a DOSBox Raw OPL capture, the game's own register stream

#include <SDL3/SDL.h>

extern "C" {
#include "opl3/opl3.h"
}

#include "audio/sr2_mus.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using sr2mus::RegWrite;

namespace {

constexpr int kSampleRate = sr2mus::kOplRate;

std::vector<uint8_t> ReadFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return {};
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) { std::fclose(f); return {}; }
    std::vector<uint8_t> out(static_cast<size_t>(n));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (got != out.size()) return {};
    return out;
}

bool BuildFromDro(const std::vector<uint8_t>& d, std::vector<RegWrite>& out, std::string& err) {
    if (d.size() < 26 || std::memcmp(d.data(), "DBRAWOPL", 8) != 0) {
        err = "not a DRO file";
        return false;
    }
    const int major = d[8] | (d[9] << 8);
    if (major != 2) { err = "only DRO v2 is supported"; return false; }
    const uint32_t pairs = d[12] | (d[13] << 8) | (d[14] << 16) | (d[15] << 24);
    const uint8_t short_code = d[23], long_code = d[24], cmlen = d[25];
    const uint8_t* codemap = d.data() + 26;
    const uint8_t* data = codemap + cmlen;
    if (data + pairs * 2 > d.data() + d.size()) { err = "DRO data is truncated"; return false; }

    uint32_t ms = 0;
    for (uint32_t i = 0; i < pairs; i++) {
        const uint8_t code = data[i * 2], val = data[i * 2 + 1];
        if (code == short_code) { ms += val + 1; continue; }
        if (code == long_code) { ms += (val + 1) << 8; continue; }
        const uint16_t reg = codemap[code & 0x7F] | ((code & 0x80) ? 0x100 : 0);
        uint32_t at = static_cast<uint32_t>(static_cast<uint64_t>(ms) * kSampleRate / 1000);
        if (!out.empty() && at <= out.back().sample) at = out.back().sample + 1;
        out.push_back({at, reg, val});
    }
    return true;
}

// ---------------------------------------------------------------- playback

// A label and the sample it starts at, so playback can say what is sounding.
struct Cue {
    uint32_t sample;
    std::string label;
};

struct Player {
    opl3_chip chip{};
    std::vector<RegWrite> writes;
    size_t next = 0;
    uint32_t sample = 0;
    std::atomic<uint32_t> played{0};   // read by the main thread for the cue line
    std::atomic<bool> done{false};
};

void SDLCALL AudioCallback(void* userdata, SDL_AudioStream* stream, int additional, int) {
    Player& p = *static_cast<Player*>(userdata);
    const int frames = additional / static_cast<int>(sizeof(int16_t) * 2);
    if (frames <= 0) return;
    std::vector<int16_t> buf(static_cast<size_t>(frames) * 2);

    for (int i = 0; i < frames; i++) {
        while (p.next < p.writes.size() && p.writes[p.next].sample <= p.sample) {
            OPL3_WriteReg(&p.chip, p.writes[p.next].reg, p.writes[p.next].value);
            p.next++;
        }
        int16_t s2[2] = {0, 0};
        OPL3_Generate(&p.chip, s2);
        buf[i * 2] = s2[0];
        buf[i * 2 + 1] = s2[1];
        p.sample++;
    }
    p.played.store(p.sample, std::memory_order_relaxed);
    if (p.next >= p.writes.size()) p.done = true;
    SDL_PutAudioStreamData(stream, buf.data(), static_cast<int>(buf.size() * sizeof(int16_t)));
}

// Offline render to a 16-bit stereo WAV.
bool WriteWav(const std::string& path, std::vector<RegWrite>& writes) {
    opl3_chip chip{};
    OPL3_Reset(&chip, kSampleRate);
    const uint32_t tail = kSampleRate;            // one second of decay
    const uint32_t total = (writes.empty() ? 0 : writes.back().sample) + tail;

    std::vector<int16_t> pcm(static_cast<size_t>(total) * 2);
    size_t next = 0;
    for (uint32_t i = 0; i < total; i++) {
        while (next < writes.size() && writes[next].sample <= i) {
            OPL3_WriteReg(&chip, writes[next].reg, writes[next].value);
            next++;
        }
        int16_t s2[2] = {0, 0};
        OPL3_Generate(&chip, s2);
        pcm[static_cast<size_t>(i) * 2] = s2[0];
        pcm[static_cast<size_t>(i) * 2 + 1] = s2[1];
    }

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const uint32_t bytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    const uint32_t rate = kSampleRate;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16v = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16v(1); u16v(2);
    u32(rate); u32(rate * 4); u16v(4); u16v(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(pcm.data(), 1, bytes, f);
    std::fclose(f);
    std::printf("wrote %s, %.1f s\n", path.c_str(), static_cast<double>(total) / kSampleRate);
    return true;
}

// ------------------------------------------------------------------ effects
//
// The DOS effect table, from RodLegacyResearch/docs/sr2_dos/adlib.md. Each is a
// timbre looked up in MSR.SND by name, put on a fixed OPL channel at a fixed
// level, and keyed at a fixed note - so they can be replayed here exactly.

struct Layer { const char* timbre; int ch; int volume; int note; };
struct Effect { const char* label; Layer layer[2]; };

const Effect kEffects[] = {
    {"engine, the note it holds",        {{"motor",  3, 0x7f, 0x18}, {nullptr,0,0,0}}},
    {"tyre screech (random 0x61..0x68)", {{"turn",   3, 0x75, 0x61}, {nullptr,0,0,0}}},
    {"tyre screech, another roll",       {{"turn",   3, 0x75, 0x66}, {nullptr,0,0,0}}},
    {"tyre screech, fixed low note",     {{"turn",   3, 0x75, 0x3d}, {nullptr,0,0,0}}},
    {"turn1",                            {{"turn1",  3, 0x7f, 0x0e}, {nullptr,0,0,0}}},
    {"CRASH: hitcar + crash",            {{"hitcar", 4, 0x7f, 0x14}, {"crash", 5, 0x7f, 0x19}}},
    {"hit and slide: hitcar + turn",     {{"hitcar", 4, 0x7f, 0x14}, {"turn",  3, 0x7f, 0x3d}}},
    {"hitcar alone, note 0x28",          {{"hitcar", 4, 0x7f, 0x28}, {nullptr,0,0,0}}},
    {"paint (respray)",                  {{"paint",  4, 0x6a, 0x30}, {nullptr,0,0,0}}},
    {"paint + part",                     {{"paint",  4, 0x6a, 0x30}, {"part",  5, 0x70, 0x30}}},
    {"part alone",                       {{"part",   5, 0x70, 0x30}, {nullptr,0,0,0}}},
    {"piano1",                           {{"piano1", 3, 0x65, 0x3c}, {nullptr,0,0,0}}},
};

const sr2mus::Patch* FindPatch(const std::vector<sr2mus::Patch>& ps, const char* name) {
    for (const sr2mus::Patch& p : ps) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

// The siren is the one effect the table does not fully describe.
void BuildSiren(const std::vector<sr2mus::Patch>& ps, std::vector<RegWrite>& w, uint32_t at) {
    const sr2mus::Patch* p = FindPatch(ps, "police");
    if (p == nullptr) return;
    const uint32_t total = kSampleRate * 3;
    sr2mus::EffectVoice(w, *p, 2, 0x7f, 0x50, at, total);
    // 15 frames at 70 Hz is the counter's period; it sits at the top for the
    // first eight and the bottom for the rest.
    const uint32_t frame = kSampleRate / 70;
    for (uint32_t f = 0; f * frame < total; f++) {
        sr2mus::EffectPitch(w, 2, (f % 15) < 8 ? 0x55 : 0x50, at + f * frame);
    }
}

std::vector<RegWrite> BuildEffects(const std::vector<sr2mus::Patch>& ps,
                                   std::vector<Cue>& cues) {
    std::vector<RegWrite> w;
    uint32_t at = kSampleRate / 4;
    const uint32_t hold = kSampleRate * 3 / 4;
    const uint32_t gap = kSampleRate / 2;
    for (const Effect& e : kEffects) {
        bool any = false;
        for (const Layer& l : e.layer) {
            if (l.timbre == nullptr) continue;
            const sr2mus::Patch* p = FindPatch(ps, l.timbre);
            if (p == nullptr) {
                std::printf("  %-34s MISSING timbre '%s'\n", e.label, l.timbre);
                continue;
            }
            sr2mus::EffectVoice(w, *p, l.ch, l.volume, l.note, at, hold);
            any = true;
        }
        if (any) {
            cues.push_back({at, e.label});
            at += hold + gap;
        }
    }
    cues.push_back({at, "siren: police, bent 0..5 semitones"});
    BuildSiren(ps, w, at);
    return w;
}

// Every timbre in the bank, in order, so nothing is missed because its effect
// routine has not been traced yet.
std::vector<RegWrite> BuildTimbres(const std::vector<sr2mus::Patch>& ps,
                                   std::vector<Cue>& cues) {
    std::vector<RegWrite> w;
    uint32_t at = kSampleRate / 4;
    const uint32_t hold = kSampleRate * 3 / 4;
    const uint32_t gap = kSampleRate / 3;
    for (const sr2mus::Patch& p : ps) {
        cues.push_back({at, p.name});
        sr2mus::EffectVoice(w, p, 3, 0x7f, 0x30, at, hold);
        at += hold + gap;
    }
    return w;
}

void Usage() {
    std::puts("Usage:");
    std::puts("  sr2_dos_player --mus <MSRBEG.MUS> [--snd <MSR.SND>] [--tickrate N]");
    std::puts("  sr2_dos_player --dro <capture.dro>");
    std::puts("  sr2_dos_player --sfx [--snd <MSR.SND>]      the traced effects");
    std::puts("  sr2_dos_player --timbres [--snd <MSR.SND>]  every timbre in the bank");
    std::puts("Options:");
    std::puts("  --dump         print the register stream instead of playing");
    std::puts("  --wav <file>   render to a WAV instead of playing");
    std::puts("  --tickrate N   .MUS ticks per second (default 480)");
}

}

int main(int argc, char* argv[]) {
    std::string mus, dro, snd, wav;
    int tick_rate = sr2mus::kTicksPerSecond;
    bool dump = false, sfx = false, timbres = false;

    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--mus") && i + 1 < argc) mus = argv[++i];
        else if (!std::strcmp(argv[i], "--dro") && i + 1 < argc) dro = argv[++i];
        else if (!std::strcmp(argv[i], "--snd") && i + 1 < argc) snd = argv[++i];
        else if (!std::strcmp(argv[i], "--tickrate") && i + 1 < argc) tick_rate = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--sfx")) sfx = true;
        else if (!std::strcmp(argv[i], "--timbres")) timbres = true;
        else if (!std::strcmp(argv[i], "--dump")) dump = true;
        else if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else { Usage(); return 0; }
    }
    const int modes = (!mus.empty()) + (!dro.empty()) + sfx + timbres;
    if (modes != 1) { Usage(); return 1; }

    std::vector<RegWrite> writes;
    std::vector<Cue> cues;
    if (sfx || timbres) {
        if (snd.empty()) snd = "MSR.SND";
        std::vector<uint8_t> sd = ReadFile(snd);
        std::vector<sr2mus::Patch> patches;
        if (sd.empty() || !sr2mus::LoadPatches(sd.data(), sd.size(), patches)) {
            std::fprintf(stderr, "cannot read patches from %s\n", snd.c_str());
            return 1;
        }
        std::printf("%s: %zu timbres\n", snd.c_str(), patches.size());
        writes = sfx ? BuildEffects(patches, cues) : BuildTimbres(patches, cues);
    } else if (!dro.empty()) {
        std::vector<uint8_t> d = ReadFile(dro);
        if (d.empty()) { std::fprintf(stderr, "cannot read %s\n", dro.c_str()); return 1; }
        std::string err;
        if (!BuildFromDro(d, writes, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
        std::printf("%s: %zu register writes\n", dro.c_str(), writes.size());
    } else {
        if (snd.empty()) {
            const size_t slash = mus.find_last_of("/\\");
            snd = (slash == std::string::npos ? std::string() : mus.substr(0, slash + 1)) + "MSR.SND";
        }
        std::vector<uint8_t> sd = ReadFile(snd);
        std::vector<sr2mus::Patch> patches;
        if (sd.empty() || !sr2mus::LoadPatches(sd.data(), sd.size(), patches)) {
            std::fprintf(stderr, "cannot read patches from %s\n", snd.c_str());
            return 1;
        }
        std::vector<uint8_t> md = ReadFile(mus);
        std::vector<sr2mus::Event> events;
        uint32_t length_ticks = 0;
        if (md.empty() || !sr2mus::ParseMus(md.data(), md.size(), events, length_ticks)) {
            std::fprintf(stderr, "cannot parse %s\n", mus.c_str());
            return 1;
        }
        std::printf("%s: %zu events, %u ticks (%.1f s at %d Hz), %zu patches from %s\n",
                    mus.c_str(), events.size(), length_ticks,
                    static_cast<double>(length_ticks) / tick_rate, tick_rate,
                    patches.size(), snd.c_str());
        writes = sr2mus::Build(events, patches, tick_rate);
        std::printf("  -> %zu register writes\n", writes.size());
    }

    if (!wav.empty()) {
        for (const Cue& c : cues) {
            std::printf("  %6.2f s  %s\n", static_cast<double>(c.sample) / kSampleRate,
                        c.label.c_str());
        }
        return WriteWav(wav, writes) ? 0 : 1;
    }

    if (dump) {
        for (const RegWrite& w : writes) {
            std::printf("%8.3f s  reg %03x = %02x\n",
                        static_cast<double>(w.sample) / kSampleRate, w.reg, w.value);
        }
        return 0;
    }

    Player player;
    OPL3_Reset(&player.chip, kSampleRate);
    player.writes = writes;
    const uint32_t last = writes.empty() ? 0 : writes.back().sample;

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = kSampleRate;
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, &player);
    if (!stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_ResumeAudioStreamDevice(stream);

    // Follow the playhead and name whatever is sounding.
    std::puts("Playing. Ctrl-C to stop.");
    size_t cue = 0;
    std::string shown;
    while (!player.done) {
        const uint32_t at = player.played.load(std::memory_order_relaxed);
        while (cue < cues.size() && cues[cue].sample <= at) {
            shown = cues[cue].label;
            cue++;
        }
        std::printf("\r  %6.2f s / %6.2f s  %-40s", static_cast<double>(at) / kSampleRate,
                    static_cast<double>(last) / kSampleRate, shown.c_str());
        std::fflush(stdout);
        SDL_Delay(50);
    }
    std::printf("\r  %6.2f s  done%-44s\n", static_cast<double>(last) / kSampleRate, "");

    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 0;
}
