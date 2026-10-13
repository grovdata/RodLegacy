#include "doctest/doctest.h"

#include "audio/c64_engine.h"
#include "audio/c64_sid.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> LoadD64() {
    const std::string prefixes[] = {"../", "../../"};
    for (const std::string& prefix : prefixes) {
        std::ifstream f(prefix + "RodLegacyBins/c64/sr1commodore/STREETR1.D64", std::ios::binary);
        if (f) return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    return {};
}

}  // namespace

TEST_SUITE("C64Sid") {

TEST_CASE("the four player PRGs come off STREETR1.D64 at their load addresses") {
    const std::vector<uint8_t> d64 = LoadD64();
    if (d64.empty()) {
        MESSAGE("STREETR1.D64 not found next to the build; skipping");
        return;
    }
    const struct { const char* name; uint16_t load; } kFiles[] = {
        {"seqbeg", 0xBF00}, {"seqgar1", 0xBF00}, {"seqgar2", 0xBF00}, {"wseq.o", 0xC100}};
    for (const auto& f : kFiles) {
        const std::vector<uint8_t> prg = c64::D64ExtractPrg(d64.data(), d64.size(), f.name);
        REQUIRE(prg.size() > 1000);
        CHECK((prg[0] | (prg[1] << 8)) == f.load);
        c64::Tune t;
        CHECK(t.Load(prg, 44100));
        CHECK(t.LoopEnd() > 0);
    }
    CHECK(c64::D64ExtractPrg(d64.data(), d64.size(), "nosuchfile").empty());
}

TEST_CASE("seqbeg song 10 gates and pitches all three voices and is not silent") {
    const std::vector<uint8_t> d64 = LoadD64();
    if (d64.empty()) {
        MESSAGE("STREETR1.D64 not found next to the build; skipping");
        return;
    }
    c64::Tune t;
    REQUIRE(t.Load(c64::D64ExtractPrg(d64.data(), d64.size(), "seqbeg"), 44100));
    t.recordWrites = true;
    t.Start();
    for (int i = 0; i < 300; i++) t.Frame();   // voice 2 enters at frame 168
    CHECK(t.Active());

    bool freq[3] = {}, gate[3] = {};
    for (uint16_t w : t.writes) {
        const int reg = w >> 8, value = w & 0xFF;
        if (reg < 21 && reg % 7 <= 1) freq[reg / 7] = true;
        if (reg < 21 && reg % 7 == 4 && (value & 1)) gate[reg / 7] = true;
    }
    for (int v = 0; v < 3; v++) {
        CHECK(freq[v]);
        CHECK(gate[v]);
    }

    t.recordWrites = false;
    t.Start();
    std::vector<int16_t> buf(44100 * 2);
    t.Render(buf.data(), static_cast<int>(buf.size()));
    int peak = 0;
    for (int16_t s : buf) peak = std::max(peak, std::abs(static_cast<int>(s)));
    CHECK(peak > 2000);
}

TEST_CASE("effects: crash (9) sounds and ends, held spray (4) releases after STOP") {
    const std::vector<uint8_t> d64 = LoadD64();
    if (d64.empty()) {
        MESSAGE("STREETR1.D64 not found next to the build; skipping");
        return;
    }
    c64::Tune t;
    REQUIRE(t.Load(c64::D64ExtractPrg(d64.data(), d64.size(), "seqgar1"), 44100));
    auto peak = [&](int frames) {
        std::vector<int16_t> buf(static_cast<size_t>(frames));
        t.Render(buf.data(), frames);
        int p = 0;
        for (int16_t s : buf) p = std::max(p, std::abs(static_cast<int>(s)));
        return p;
    };
    t.Start(9);
    CHECK(peak(44100) > 2000);
    peak(44100 * 2);
    CHECK_FALSE(t.Active());

    t.Start(4);
    CHECK(peak(44100) > 2000);
    t.Stop();
    peak(44100);
    CHECK(peak(4410) < 500);
}

TEST_CASE("engine: me.o pitch formula and the bay drone pitch") {
    CHECK(C64Engine::FreqForRpm(500) == 0x17D);    // the race's idle, $01F4
    CHECK(C64Engine::FreqForRpm(6000) == 0x6DC);
    CHECK(C64Engine::BayPitch(0) == 0x2C);
    CHECK(C64Engine::BayPitch(-3) == 0x20);
    CHECK(C64Engine::BayPitch(4) == 0x1C);
    CHECK(C64Engine::BayPitch(99) == 0x04);
}

TEST_CASE("engine and race effects sound: bay drone, drive in/out, bump, off-road") {
    const std::vector<uint8_t> d64 = LoadD64();
    if (d64.empty()) {
        MESSAGE("STREETR1.D64 not found next to the build; skipping");
        return;
    }
    c64::Tune t;
    REQUIRE(t.Load(c64::D64ExtractPrg(d64.data(), d64.size(), "seqgar1"), 44100));
    std::vector<int16_t> buf(44100);
    for (int song : {11, 12, 16, 17, 18}) {
        t.Poke(static_cast<uint16_t>(t.Base() + 0x12), C64Engine::BayPitch(0));
        t.Start(song);
        t.Render(buf.data(), static_cast<int>(buf.size()));
        int peak = 0;
        for (int16_t s : buf) peak = std::max(peak, std::abs(static_cast<int>(s)));
        CHECK_MESSAGE(peak > 2000, "song ", song);
    }
}

TEST_CASE("engine: the race voice sounds after a drive-in scene") {
    const std::vector<uint8_t> d64 = LoadD64();
    if (d64.empty()) {
        MESSAGE("STREETR1.D64 not found next to the build; skipping");
        return;
    }
    c64::Tune t;
    REQUIRE(t.Load(c64::D64ExtractPrg(d64.data(), d64.size(), "seqgar1"), 44100));
    std::vector<int16_t> buf(44100 * 4);
    t.Start(12);
    t.Render(buf.data(), static_cast<int>(buf.size()));
    C64Engine::LoadRace(t);
    const uint16_t f = C64Engine::FreqForRpm(1500);
    t.WriteSid(0, static_cast<uint8_t>(f));
    t.WriteSid(1, static_cast<uint8_t>(f >> 8));
    buf.assign(44100, 0);
    t.Render(buf.data(), static_cast<int>(buf.size()));
    int peak = 0;
    for (int i = 22050; i < 44100; i++) peak = std::max(peak, std::abs(static_cast<int>(buf[static_cast<size_t>(i)])));
    CHECK(peak > 2000);
}

}
