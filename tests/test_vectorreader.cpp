#include <doctest/doctest.h>
#include "parsers/libp.h"

TEST_SUITE("VectorReader") {

TEST_CASE("ReadUint8 basic") {
    std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF};
    VectorReader r(data);
    CHECK(r.ReadUint8() == 0xAB);
    CHECK(r.ReadUint8() == 0xCD);
    CHECK(r.ReadUint8() == 0xEF);
}

TEST_CASE("ReadUint8 advances position") {
    std::vector<uint8_t> data = {0x01, 0x02};
    VectorReader r(data);
    r.ReadUint8();
    CHECK(r.Tell() == 1);
}

TEST_CASE("ReadUint8 out of bounds throws") {
    std::vector<uint8_t> data = {0x01};
    VectorReader r(data);
    r.ReadUint8();
    CHECK_THROWS_AS(r.ReadUint8(), std::runtime_error);
}

TEST_CASE("ReadUint16BE big-endian") {
    std::vector<uint8_t> data = {0x01, 0x02};
    VectorReader r(data);
    CHECK(r.ReadUint16BE() == 0x0102);
}

TEST_CASE("ReadUint16BE advances position by 2") {
    std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0x03};
    VectorReader r(data);
    r.ReadUint16BE();
    CHECK(r.Tell() == 2);
}

TEST_CASE("ReadUint16BE out of bounds throws") {
    std::vector<uint8_t> data = {0x01};
    VectorReader r(data);
    CHECK_THROWS_AS(r.ReadUint16BE(), std::runtime_error);
}

TEST_CASE("ReadUint16LE little-endian") {
    std::vector<uint8_t> data = {0x01, 0x02};
    VectorReader r(data);
    CHECK(r.ReadUint16LE() == 0x0201);
}

TEST_CASE("ReadUint16LE distinguishes from BE") {
    std::vector<uint8_t> data = {0xAB, 0xCD};
    VectorReader r(data);
    CHECK(r.ReadUint16LE() == 0xCDAB);
}

TEST_CASE("ReadUint32BE big-endian") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    VectorReader r(data);
    CHECK(r.ReadUint32BE() == 0x01020304u);
}

TEST_CASE("ReadUint32BE advances position by 4") {
    std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0xFF};
    VectorReader r(data);
    r.ReadUint32BE();
    CHECK(r.Tell() == 4);
}

TEST_CASE("ReadUint32BE out of bounds throws") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    VectorReader r(data);
    CHECK_THROWS_AS(r.ReadUint32BE(), std::runtime_error);
}

TEST_CASE("ReadUint32LE little-endian") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    VectorReader r(data);
    CHECK(r.ReadUint32LE() == 0x04030201u);
}

TEST_CASE("ReadNullTerminatedString basic") {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o', 0x00, 0xFF};
    VectorReader r(data);
    std::string s = r.ReadNullTerminatedString();
    CHECK(s == "Hello");
    CHECK(r.Tell() == 6); // points past the null terminator
}

TEST_CASE("ReadNullTerminatedString empty string") {
    std::vector<uint8_t> data = {0x00, 'A'};
    VectorReader r(data);
    std::string s = r.ReadNullTerminatedString();
    CHECK(s == "");
    CHECK(r.Tell() == 1);
}

TEST_CASE("ReadBytes returns correct slice") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    VectorReader r(data);
    auto bytes = r.ReadBytes(3);
    REQUIRE(bytes.size() == 3);
    CHECK(bytes[0] == 0x01);
    CHECK(bytes[1] == 0x02);
    CHECK(bytes[2] == 0x03);
    CHECK(r.Tell() == 3);
}

TEST_CASE("ReadBytes out of bounds throws") {
    std::vector<uint8_t> data = {0x01, 0x02};
    VectorReader r(data);
    CHECK_THROWS_AS(r.ReadBytes(3), std::runtime_error);
}

TEST_CASE("Seek and Tell") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    VectorReader r(data);
    r.Seek(2);
    CHECK(r.Tell() == 2);
    CHECK(r.ReadUint8() == 0x03);
}

TEST_CASE("Seek to zero resets position") {
    std::vector<uint8_t> data = {0xAA, 0xBB};
    VectorReader r(data);
    r.ReadUint8();
    r.Seek(0);
    CHECK(r.ReadUint8() == 0xAA);
}

TEST_CASE("Seek beyond bounds throws") {
    std::vector<uint8_t> data = {0x01};
    VectorReader r(data);
    CHECK_THROWS_AS(r.Seek(10), std::runtime_error);
}

TEST_CASE("Sequential reads advance position correctly") {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    VectorReader r(data);
    r.ReadUint8();
    r.ReadUint16BE();
    r.ReadUint32BE();
    CHECK(r.Tell() == 7);
    CHECK(r.ReadUint8() == 0x08);
}

}
