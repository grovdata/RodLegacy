#include <doctest/doctest.h>
#include "parsers/libp.h"
#include <cstring>

TEST_SUITE("SrLibDataCleanupCarString") {

TEST_CASE("double backticks replaced by single space") {
    char s[] = "1959``Chevrolet";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "1959 Chevrolet");
}

TEST_CASE("single backtick is preserved (narrow space handled by SFont)") {
    char s[] = "Hot`Rod";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Hot`Rod");
}

TEST_CASE("leading plus sign is stripped (field type marker)") {
    char s[] = "+Hot Rod";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Hot Rod");
}

TEST_CASE("plus sign mid-string is preserved") {
    char s[] = "Hot+Rod";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Hot+Rod");
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

TEST_CASE("double backtick to space, single backtick and mid-plus preserved") {
    char s[] = "A``B`C+D";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "A B`C+D");
}

TEST_CASE("trailing period is preserved") {
    char s[] = "Nice car.";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Nice car.");
}

TEST_CASE("leading double plus: first stripped, rest preserved") {
    char s[] = "++B";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "+B");
}

TEST_CASE("double backtick at end of string") {
    char s[] = "Ford``";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "Ford ");
}

TEST_CASE("mixed backtick types") {
    // double backtick becomes space, single backtick is preserved
    char s[] = "A``B`C";
    SrLibDataCleanupCarString(s);
    CHECK(std::string(s) == "A B`C");
}

}
