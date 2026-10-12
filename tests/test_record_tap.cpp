#include <doctest/doctest.h>

#include "audio/record_tap.h"

#include <filesystem>

TEST_CASE("record_tap pads a late source back onto the wall clock") {
    namespace fs = std::filesystem;
    const std::string base = (fs::temp_directory_path() / "rodlegacy_tap_test").string();
    record_tap::Start(base);
    record_tap::startMs = record_tap::NowMs() - 1000;   // the source first speaks a second in

    const int16_t chunk[4] = {1, 2, 3, 4};
    record_tap::Write("t", 1000, 2, chunk, sizeof(chunk));
    record_tap::Write("t", 1000, 2, chunk, sizeof(chunk));   // continuous: appended, not padded again
    const auto out = record_tap::Stop();

    REQUIRE(out.size() == 1);
    const auto size = fs::file_size(out[0].path);
    CHECK(size >= 1000 * 4 + 2 * sizeof(chunk));
    CHECK(size <= 1010 * 4 + 2 * sizeof(chunk));
    CHECK((size % 4) == 0);
    fs::remove(out[0].path);

    record_tap::Write("t", 1000, 2, chunk, sizeof(chunk));   // stopped: ignored
    CHECK(!fs::exists(out[0].path));
}
