#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace balsa::visualization::shaders {

enum class EmbeddedShader {
    FlatVertex,
    FlatFragment,
    TriangleVertex,
    TriangleFragment,
    MeshVertex,
    MeshFragment,
    ImageVertex,
    ImageFragment,
};

auto embedded_shader_source(EmbeddedShader shader) -> std::string_view;
auto colormap_shader_source(std::string_view name) -> std::optional<std::string_view>;
auto colormap_names() -> std::span<const std::string_view>;

}// namespace balsa::visualization::shaders
