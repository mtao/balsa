#include <balsa/string/capitalization.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

namespace ascii = balsa::string::ascii;

TEST_CASE("ascii_case_conversion", "[string]") {
    STATIC_CHECK(ascii::tolower('A') == 'a');
    STATIC_CHECK(ascii::tolower('z') == 'z');
    STATIC_CHECK(ascii::toupper('q') == 'Q');
    STATIC_CHECK(ascii::tolower('@') == '@');
    STATIC_CHECK(ascii::tolower(".OBJ") == ".obj");
    STATIC_CHECK(ascii::toupper("glTF-2") == "GLTF-2");
    STATIC_CHECK(ascii::IgnoreCaseEqual{}("Image/PNG", "image/png"));
    STATIC_CHECK_FALSE(ascii::IgnoreCaseEqual{}("png", "pngx"));
    STATIC_CHECK_FALSE(ascii::IgnoreCaseEqual{}("png", "jpg"));

    // Non-ASCII bytes (here UTF-8 "É") are left unchanged.
    CHECK(ascii::tolower("\xC3\x89T\xC3\x89") == "\xC3\x89t\xC3\x89");

    std::string s = "MiXeD";
    ascii::tolower_in_place(s);
    CHECK(s == "mixed");
    ascii::toupper_in_place(s);
    CHECK(s == "MIXED");
}

TEST_CASE("ascii_case_insensitive_order", "[string]") {
    STATIC_CHECK(ascii::compare_ignore_case("OBJ", "obj") == 0);
    STATIC_CHECK(ascii::compare_ignore_case("Glb", "gltf") < 0);
    STATIC_CHECK(ascii::compare_ignore_case("b", "A") > 0);
    STATIC_CHECK(ascii::compare_ignore_case("ab", "AbC") < 0);
    STATIC_CHECK(ascii::compare_ignore_case("", "") == 0);
    // 'Z' < 'a' in ASCII, but z > a ignoring case.
    STATIC_CHECK(ascii::IgnoreCaseLess{}("a", "Z"));

    std::map<std::string, int, ascii::IgnoreCaseLess> by_extension{
        {".obj", 1}, {".GLB", 2}};
    CHECK(by_extension.contains(std::string_view(".OBJ")));
    CHECK(by_extension.find(".glb")->second == 2);
    by_extension[".Obj"] = 3; // same key
    CHECK(by_extension.size() == 2);
    CHECK(by_extension.at(".obj") == 3);
}
