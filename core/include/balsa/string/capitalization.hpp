#pragma once
#include <algorithm>
#include <compare>
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
    // Lexicographic order of the lowercased strings.  Weak: strings that
    // differ only in case are equivalent.
    constexpr auto compare_ignore_case(std::string_view a, std::string_view b)
        -> std::weak_ordering {
        return std::lexicographical_compare_three_way(
            a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
                return std::weak_order(tolower(x), tolower(y));
            });
    }

    // Case-insensitive comparators, transparent for string_view lookups,
    // e.g. std::map<std::string, T, ascii::IgnoreCaseLess>.
    struct IgnoreCaseEqual {
        using is_transparent = void;
        static constexpr auto operator()(std::string_view a, std::string_view b)
            -> bool {
            return std::ranges::equal(
                a, b, [](char x, char y) { return tolower(x) == tolower(y); });
        }
    };

    struct IgnoreCaseLess {
        using is_transparent = void;
        static constexpr auto operator()(std::string_view a, std::string_view b)
            -> bool {
            return compare_ignore_case(a, b) < 0;
        }
    };
} // namespace ascii
} // namespace balsa::string
