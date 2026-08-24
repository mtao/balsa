#include "balsa/visualization/shaders/embedded_sources.hpp"

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

}// namespace

auto embedded_shader_source(EmbeddedShader shader) -> std::string_view {
    switch (shader) {
    case EmbeddedShader::FlatVertex: return as_string_view(flat_vertex);
    case EmbeddedShader::FlatFragment: return as_string_view(flat_fragment);
    case EmbeddedShader::TriangleVertex: return as_string_view(triangle_vertex);
    case EmbeddedShader::TriangleFragment: return as_string_view(triangle_fragment);
    case EmbeddedShader::MeshVertex: return as_string_view(mesh_vertex);
    case EmbeddedShader::MeshFragment: return as_string_view(mesh_fragment);
    case EmbeddedShader::ImageVertex: return as_string_view(image_vertex);
    case EmbeddedShader::ImageFragment: return as_string_view(image_fragment);
    }
    return {};
}

}// namespace balsa::visualization::shaders
