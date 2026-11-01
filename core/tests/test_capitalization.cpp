#include <balsa/string/capitalization.hpp>
#include <catch2/catch_test_macros.hpp>

namespace ascii = balsa::string::ascii;

TEST_CASE("ascii_case_conversion", "[string]") {
    STATIC_CHECK(ascii::tolower('A') == 'a');
    STATIC_CHECK(ascii::tolower('z') == 'z');
    STATIC_CHECK(ascii::toupper('q') == 'Q');
    STATIC_CHECK(ascii::tolower('@') == '@');
    STATIC_CHECK(ascii::tolower(".OBJ") == ".obj");
    STATIC_CHECK(ascii::toupper("glTF-2") == "GLTF-2");
    STATIC_CHECK(ascii::equals_ignore_case("Image/PNG", "image/png"));
    STATIC_CHECK_FALSE(ascii::equals_ignore_case("png", "pngx"));
    STATIC_CHECK_FALSE(ascii::equals_ignore_case("png", "jpg"));

    // Non-ASCII bytes (here UTF-8 "É") are left unchanged.
    CHECK(ascii::tolower("\xC3\x89T\xC3\x89") == "\xC3\x89t\xC3\x89");

    std::string s = "MiXeD";
    ascii::tolower_in_place(s);
    CHECK(s == "mixed");
    ascii::toupper_in_place(s);
    CHECK(s == "MIXED");
}
