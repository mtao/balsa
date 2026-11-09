#pragma once
#include "../Extension.hpp"

#include <balsa/json/reflection.hpp>
#include <nlohmann/json.hpp>

#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace balsa::visualization::gltf::detail {

// The first of `names` present in `extensions`, parsed as T (which has a
// from_json); nullopt if none is present, an error message if malformed.
template <typename T>
auto parse_extension(const Extension &extensions,
                     std::span<const std::string_view> names)
    -> std::expected<std::optional<T>, std::string> {
    if (extensions.empty()) return std::nullopt;
    const auto parsed = nlohmann::json::parse(extensions.text, nullptr, false);
    if (!parsed.is_object()) return std::nullopt;
    for (const auto name : names) {
        const auto it = parsed.find(name);
        if (it == parsed.end()) continue;
        try {
            return it->template get<T>();
        } catch (const std::exception &e) {
            return std::unexpected(std::format("invalid {}: {}", name, e.what()));
        }
    }
    return std::nullopt;
}

} // namespace balsa::visualization::gltf::detail
