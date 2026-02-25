#include <doctest/doctest.h>
#include "parsers/libp.h"
#include <cstring>

// ---------------------------------------------------------------------------
// SrLibDataCleanupCarString
// ---------------------------------------------------------------------------

TEST_SUITE("SrLibDataCleanupCarString") {

TEST_CASE("double backticks replaced by single space") {
    char s[] = "1959``Chevrolet";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "1959 Chevrolet");
}

TEST_CASE("single backtick replaced by space") {
    char s[] = "Hot`Rod";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Hot Rod");
}

TEST_CASE("plus sign replaced by space") {
    char s[] = "Hot+Rod";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Hot Rod");
}

TEST_CASE("string with no special chars is unchanged") {
    char s[] = "1959 Chevrolet Impala";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "1959 Chevrolet Impala");
}

TEST_CASE("empty string is unchanged") {
    char s[] = "";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "");
}

TEST_CASE("multiple special chars all replaced") {
    char s[] = "A``B`C+D";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "A B C D");
}

TEST_CASE("trailing period is preserved") {
    char s[] = "Nice car.";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Nice car.");
}

TEST_CASE("consecutive plus signs each become a space") {
    char s[] = "A++B";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "A  B");
}

TEST_CASE("double backtick at end of string") {
    char s[] = "Ford``";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Ford ");
}

TEST_CASE("mixed backtick types") {
    // double backtick first, then single
    char s[] = "A``B`C";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "A B C");
}

} // TEST_SUITE
