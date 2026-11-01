#pragma once
#include <algorithm>
#include <string>
#include <string_view>

namespace balsa::string {
// Didn't see stdlib inlining tolower/toupper so it's easier to handroll my own
namespace ascii {
    constexpr auto tolower(char c) -> char {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }
    constexpr auto toupper(char c) -> char {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }
    constexpr void tolower_in_place(std::string &s) {
        std::ranges::transform(s, s.begin(), [](char c) { return tolower(c); });
    }
    constexpr void toupper_in_place(std::string &s) {
        std::ranges::transform(s, s.begin(), [](char c) { return toupper(c); });
    }
    constexpr auto tolower(std::string_view s) -> std::string {
        // TODO: properly benchmark vs transform(tolower) | to<string>()
        std::string r(s);
        tolower_in_place(r);
        return r;
    }
    constexpr auto toupper(std::string_view s) -> std::string {
        // TODO: properly benchmark vs transform(toupper) | to<string>()
        std::string r(s);
        toupper_in_place(r);
        return r;
    }
    constexpr auto equals_ignore_case(std::string_view a, std::string_view b)
        -> bool {
        return std::ranges::equal(
            a, b, [](char x, char y) { return tolower(x) == tolower(y); });
    }
} // namespace ascii
} // namespace balsa::string
