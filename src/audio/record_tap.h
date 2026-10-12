#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// While F6 records, every audio device copies its S16 output here, one raw file per source. A source that fell
// silent (paused device, empty queue) is padded back onto the wall clock, so main.cpp can mix them over the video.
namespace record_tap {

struct Source {
    FILE* file = nullptr;
    std::string path;
    int freq = 0;
    int channels = 0;
    uint64_t bytes = 0;
};

inline std::atomic<bool> active{false};
// The mingw toolchain has no std::mutex; the audio threads hold this only for a write.
inline std::atomic_flag busy = ATOMIC_FLAG_INIT;

struct Guard {
    Guard() { while (busy.test_and_set(std::memory_order_acquire)) {} }
    ~Guard() { busy.clear(std::memory_order_release); }
};
inline std::map<std::string, Source> sources;
inline std::string base;
inline uint64_t startMs = 0;

inline uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

inline void Start(const std::string& name) {
    Guard g;
    base = name;
    startMs = NowMs();
    sources.clear();
    active = true;
}

inline void Write(const char* name, int freq, int channels, const void* data, int len) {
    if (!active.load(std::memory_order_relaxed) || len <= 0) return;
    Guard g;
    if (!active) return;
    Source& s = sources[name];
    if (!s.file) {
        s.path = base + "." + name + ".pcm";
        s.file = std::fopen(s.path.c_str(), "wb");
        s.freq = freq;
        s.channels = channels;
        if (!s.file) return;
    }
    const uint64_t frame = static_cast<uint64_t>(s.channels) * 2;
    const uint64_t wall = (NowMs() - startMs) * static_cast<uint64_t>(s.freq) / 1000 * frame;
    const uint64_t slack = static_cast<uint64_t>(s.freq) / 5 * frame;
    if (s.bytes + slack < wall) {
        static const char zeros[4096] = {};
        for (uint64_t pad = wall - s.bytes; pad > 0;) {
            const size_t n = static_cast<size_t>(std::min<uint64_t>(pad, sizeof(zeros)));
            std::fwrite(zeros, 1, n, s.file);
            pad -= n;
        }
        s.bytes = wall;
    }
    std::fwrite(data, 1, static_cast<size_t>(len), s.file);
    s.bytes += static_cast<uint64_t>(len);
}

inline std::vector<Source> Stop() {
    Guard g;
    active = false;
    std::vector<Source> out;
    for (auto& [name, s] : sources) {
        if (s.file) {
            std::fclose(s.file);
            out.push_back(s);
        }
    }
    sources.clear();
    return out;
}

}  // namespace record_tap
