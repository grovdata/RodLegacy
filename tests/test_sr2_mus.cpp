#include "doctest/doctest.h"
#include "audio/adlib_music.h"
#include "audio/sr2_mus.h"

#include <cstring>

// The SR2 DOS music format: MSR.SND patches and the .MUS event stream, as
// decoded in RodLegacyResearch/docs/sr2_dos/adlib.md.

namespace {

void PutField(std::vector<uint8_t>& d, size_t at, int i, int16_t v) {
    d[at + i * 2]     = static_cast<uint8_t>(v & 0xFF);
    d[at + i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// One instrument named "testpat", so the field-to-register mapping can be
// checked against known values.
std::vector<uint8_t> MakeSnd() {
    const int count = 1;
    const int patchAt = 6 + count * 9;
    std::vector<uint8_t> d(static_cast<size_t>(patchAt) + 56, 0);
    d[0] = 1;                                     // version
    d[2] = static_cast<uint8_t>(count);
    d[4] = static_cast<uint8_t>(patchAt & 0xFF);
    d[5] = static_cast<uint8_t>(patchAt >> 8);
    std::memcpy(&d[6], "testpat", 7);

    const size_t at = static_cast<size_t>(patchAt);
    PutField(d, at, 0, 1);      // op1 KSL
    PutField(d, at, 1, 3);      // op1 multiple
    PutField(d, at, 2, 3);      // feedback
    PutField(d, at, 3, 15);     // op1 attack
    PutField(d, at, 5, 1);      // op1 EG sustaining
    PutField(d, at, 8, 11);     // op1 total level, used as stored
    PutField(d, at, 11, 1);     // op1 KSR
    PutField(d, at, 12, 0);     // connection field 0 means FM (bit 1)
    PutField(d, at, 13 + 0, 2); // op2 KSL
    PutField(d, at, 13 + 1, 1); // op2 multiple
    PutField(d, at, 13 + 8, 0); // op2 total level, the volume control
    PutField(d, at, 26, 1);     // op1 waveform
    PutField(d, at, 27, 3);     // op2 waveform
    return d;
}

// Program change, note on, then note off 12 ticks later with two 0xF8 clock
// bytes in that delta.
std::vector<uint8_t> MakeMus() {
    std::vector<uint8_t> d(70, 0);
    d[0x26] = 12;
    const uint8_t events[] = {
        0x00, 0xC0, 0x00,
        0x00, 0x90, 60, 127,
        0xF8, 0xF8, 0x0A, 0x80, 60, 0,
    };
    d.insert(d.end(), events, events + sizeof(events));
    return d;
}

const sr2mus::RegWrite* Find(const std::vector<sr2mus::RegWrite>& w, uint16_t reg) {
    for (const sr2mus::RegWrite& x : w) {
        if (x.reg == reg) return &x;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("MSR.SND patch fields land on the right operator parameters") {
    std::vector<uint8_t> snd = MakeSnd();
    std::vector<sr2mus::Patch> patches;
    REQUIRE(sr2mus::LoadPatches(snd.data(), snd.size(), patches));
    REQUIRE(patches.size() == 1);

    const sr2mus::Patch& p = patches[0];
    CHECK(p.name == "testpat");
    CHECK(p.op[0].ksl == 1);
    CHECK(p.op[0].mult == 3);
    CHECK(p.op[0].ar == 15);
    CHECK(p.op[0].eg == 1);
    CHECK(p.op[0].tl == 11);
    CHECK(p.op[0].ksr == 1);
    CHECK(p.op[0].ws == 1);
    CHECK(p.op[1].ksl == 2);
    CHECK(p.op[1].mult == 1);
    CHECK(p.op[1].ws == 3);
    CHECK(p.feedback == 3);
    CHECK(p.connection == 1);   // field 12 is inverted

    // A header whose patch offset does not match the name table is not MSR.SND.
    snd[4] = 99;
    CHECK_FALSE(sr2mus::LoadPatches(snd.data(), snd.size(), patches));
}

TEST_CASE(".MUS deltas are single bytes and 0xF8 counts as one tick") {
    std::vector<uint8_t> mus = MakeMus();
    std::vector<sr2mus::Event> events;
    uint32_t lengthTicks = 0;
    REQUIRE(sr2mus::ParseMus(mus.data(), mus.size(), events, lengthTicks));
    REQUIRE(events.size() == 3);
    CHECK(lengthTicks == 12);

    CHECK(events[0].status == 0xC0);
    CHECK(events[0].tick == 0);
    CHECK(events[1].status == 0x90);
    CHECK(events[1].a == 60);
    CHECK(events[1].b == 127);
    CHECK(events[1].tick == 0);
    CHECK(events[2].status == 0x80);
    CHECK(events[2].tick == 12);   // two 0xF8 bytes plus a delta of 10
}

TEST_CASE("a melodic note becomes a keyed OPL voice and the note off clears it") {
    std::vector<uint8_t> snd = MakeSnd();
    std::vector<uint8_t> mus = MakeMus();
    std::vector<sr2mus::Patch> patches;
    std::vector<sr2mus::Event> events;
    uint32_t lengthTicks = 0;
    REQUIRE(sr2mus::LoadPatches(snd.data(), snd.size(), patches));
    REQUIRE(sr2mus::ParseMus(mus.data(), mus.size(), events, lengthTicks));

    std::vector<sr2mus::RegWrite> w = sr2mus::Build(events, patches);
    REQUIRE(w.size() > 2);

    // Waveform select first, then rhythm mode, as the game does.
    CHECK(w[0].reg == 0x01);
    CHECK(w[0].value == 0x20);
    CHECK(w[1].reg == 0xBD);
    CHECK(w[1].value == 0x20);

    // Channel 0's operators are slots 0 and 3. The modulator's total level
    // comes through as stored, with its KSL above it.
    const sr2mus::RegWrite* mod = Find(w, 0x40 + 0);
    REQUIRE(mod != nullptr);
    CHECK(mod->value == ((1 << 6) | 11));
    // Feedback 3 with connection 1 in register 0xC0.
    const sr2mus::RegWrite* conn = Find(w, 0xC0);
    REQUIRE(conn != nullptr);
    CHECK(conn->value == ((3 << 1) | 1));
    // Note 60 is F-number 343 in block 4, keyed on.
    const sr2mus::RegWrite* lo = Find(w, 0xA0);
    REQUIRE(lo != nullptr);
    CHECK(lo->value == (343 & 0xFF));
    const sr2mus::RegWrite* hi = Find(w, 0xB0);
    REQUIRE(hi != nullptr);
    CHECK(hi->value == (0x20 | (4 << 2) | (343 >> 8)));

    // The note off is the last write, a bare 0xB0 = 0 one second later at 480
    // ticks per second.
    CHECK(w.back().reg == 0xB0);
    CHECK(w.back().value == 0);
    CHECK(w.back().sample > static_cast<uint32_t>(sr2mus::kOplRate) * 12 / 480 - 2);

    // Every write is strictly later than the one before it: a key off has to be
    // seen before the key on that follows, so writes never share a sample.
    for (size_t i = 1; i < w.size(); i++) {
        CHECK(w[i].sample > w[i - 1].sample);
    }
}

TEST_CASE("the music gain lifts the OPL to the other sources and clamps") {
    const int full = AdlibMusic::VolumeScale(1.0f);
    CHECK(full > 256);                                  // louder than the chip alone
    CHECK(AdlibMusic::VolumeScale(0.5f) == full / 2);
    CHECK(AdlibMusic::VolumeScale(0.0f) == 0);
    CHECK(AdlibMusic::VolumeScale(2.0f) == full);        // clamped
    CHECK(AdlibMusic::VolumeScale(-1.0f) == 0);

    // MSR20 is the loudest of the five songs, peaking at 11174 in a WAV render.
    CHECK(AdlibMusic::Scale(11174, full) > 30000);
    CHECK(AdlibMusic::Scale(11174, full) < 32767);
    CHECK(AdlibMusic::Scale(-11174, full) > -32768);
    CHECK(AdlibMusic::Scale(30000, 0) == 0);

    // A louder sample than any song clamps instead of wrapping to the far rail.
    CHECK(AdlibMusic::Scale(20000, full) == 32767);
    CHECK(AdlibMusic::Scale(-20000, full) == -32768);
}

TEST_CASE("percussion is triggered through register 0xBD") {
    std::vector<uint8_t> snd = MakeSnd();
    std::vector<sr2mus::Patch> patches;
    REQUIRE(sr2mus::LoadPatches(snd.data(), snd.size(), patches));

    // Channel 6 is the bass drum, bit 4 of 0xBD. Note on then note off.
    std::vector<sr2mus::Event> events = {
        {0, 0xC6, 0, 0},
        {0, 0x96, 36, 100},
        {240, 0x86, 36, 0},
    };
    std::vector<sr2mus::RegWrite> w = sr2mus::Build(events, patches);

    bool sawDrop = false, sawTrigger = false, sawRelease = false;
    for (size_t i = 2; i < w.size(); i++) {   // skip the two setup writes
        if (w[i].reg != 0xBD) continue;
        if (w[i].value == 0x20) sawDrop = true;
        if (w[i].value == (0x20 | 0x10)) sawTrigger = true;
        if (sawTrigger && w[i].value == 0x20) sawRelease = true;
    }
    CHECK(sawDrop);        // the bit drops before it rises, so the voice retriggers
    CHECK(sawTrigger);
    CHECK(sawRelease);
}

namespace {
std::vector<sr2mus::Patch> FxBank() {
    std::vector<sr2mus::Patch> bank;
    for (const char* n : {"hitcar", "crash", "turn", "turn1", "part", "paint", "police"}) {
        sr2mus::Patch p;
        p.name = n;
        bank.push_back(p);
    }
    return bank;
}

int KeyOns(const std::vector<sr2mus::RegWrite>& w, int ch) {
    int n = 0;
    for (const sr2mus::RegWrite& r : w) {
        if (r.reg == 0xB0 + ch && (r.value & 0x20)) ++n;
    }
    return n;
}
}

TEST_CASE("AdLib crash keys hitcar on voice 4 and crash on voice 5, then lets both go") {
    const auto w = adlibfx::Build(adlibfx::Sound::Crash, FxBank(), 0, 0);
    CHECK(KeyOns(w, 4) == 1);
    CHECK(KeyOns(w, 5) == 1);
    bool off4 = false, off5 = false;
    for (const sr2mus::RegWrite& r : w) {
        if (r.reg == 0xB4 && r.value == 0) off4 = true;
        if (r.reg == 0xB5 && r.value == 0) off5 = true;
    }
    CHECK(off4);
    CHECK(off5);
}

TEST_CASE("AdLib squeal pulses 8 ticks on, 8 off") {
    const auto w = adlibfx::Build(adlibfx::Sound::Squeal, FxBank(), 1.0, 0);
    CHECK(KeyOns(w, 5) == 5);   // one second is 72.8 ticks: pulses at 0, 16, 32, 48, 64
}

TEST_CASE("AdLib siren bends inside its time and writes in sample order") {
    const auto w = adlibfx::Build(adlibfx::Sound::Siren, FxBank(), 1.0, 0);
    CHECK(KeyOns(w, 2) > 1);
    for (size_t i = 1; i < w.size(); ++i) CHECK(w[i - 1].sample <= w[i].sample);
    CHECK(w.back().sample <= static_cast<uint32_t>(sr2mus::kOplRate) + 32);   // writes are spaced a sample apart
}

TEST_CASE("an AdLib effect with its timbre missing from the bank is silent") {
    CHECK(adlibfx::Build(adlibfx::Sound::Thud, {}, 0, 0).empty());
}
