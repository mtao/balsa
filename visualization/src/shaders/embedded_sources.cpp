#include "balsa/visualization/shaders/embedded_sources.hpp"

#include <array>

namespace balsa::visualization::shaders {
namespace {

constexpr unsigned char flat_vertex[] = {
#embed "flat.vert"
};
constexpr unsigned char flat_fragment[] = {
#embed "flat.frag"
};
constexpr unsigned char triangle_vertex[] = {
#embed "triangle.vert"
};
constexpr unsigned char triangle_fragment[] = {
#embed "triangle.frag"
};
constexpr unsigned char mesh_vertex[] = {
#embed "mesh.vert"
};
constexpr unsigned char mesh_fragment[] = {
#embed "mesh.frag"
};
constexpr unsigned char image_vertex[] = {
#embed "image.vert"
};
constexpr unsigned char image_fragment[] = {
#embed "image.frag"
};

template<std::size_t N>
auto as_string_view(const unsigned char (&data)[N]) -> std::string_view {
    return {reinterpret_cast<const char *>(data), N};
}

struct NamedSource {
    std::string_view path;
    std::string_view source;
};

const std::array shader_sources{
    NamedSource{"flat/vertex", as_string_view(flat_vertex)},
    NamedSource{"flat/fragment", as_string_view(flat_fragment)},
    NamedSource{"examples/triangle/vertex", as_string_view(triangle_vertex)},
    NamedSource{"examples/triangle/fragment", as_string_view(triangle_fragment)},
    NamedSource{"mesh/vertex", as_string_view(mesh_vertex)},
    NamedSource{"mesh/fragment", as_string_view(mesh_fragment)},
    NamedSource{"image/vertex", as_string_view(image_vertex)},
    NamedSource{"image/fragment", as_string_view(image_fragment)},
};

}// namespace

auto shader_source(std::string_view path) -> std::optional<std::string_view> {
    for (const auto &entry : shader_sources) {
        if (entry.path == path) {
            return entry.source;
        }
    }
    return std::nullopt;
}

}// namespace balsa::visualization::shaders
