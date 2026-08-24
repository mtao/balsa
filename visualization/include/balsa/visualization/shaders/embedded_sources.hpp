#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace balsa::visualization::shaders {

auto shader_source(std::string_view path) -> std::optional<std::string_view>;
auto colormap_shader_source(std::string_view name) -> std::optional<std::string_view>;
auto colormap_names() -> std::span<const std::string_view>;

}// namespace balsa::visualization::shaders
