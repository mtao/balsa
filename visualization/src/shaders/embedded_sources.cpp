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

auto builtin_shader_library() -> const ShaderLibrary & {
    static ShaderLibrary library;
    static const bool initialized = [&] {
        (void)library.add_shader("flat/vertex", std::string{as_string_view(flat_vertex)});
        (void)library.add_shader("flat/fragment", std::string{as_string_view(flat_fragment)});
        (void)library.add_shader(
          "examples/triangle/vertex", std::string{as_string_view(triangle_vertex)});
        (void)library.add_shader(
          "examples/triangle/fragment", std::string{as_string_view(triangle_fragment)});
        (void)library.add_shader("mesh/vertex", std::string{as_string_view(mesh_vertex)});
        (void)library.add_shader("mesh/fragment", std::string{as_string_view(mesh_fragment)});
        (void)library.add_shader("image/vertex", std::string{as_string_view(image_vertex)});
        (void)library.add_shader("image/fragment", std::string{as_string_view(image_fragment)});
        library.make_read_only();
        return true;
    }();
    (void)initialized;
    return library;
}

}// namespace balsa::visualization::shaders
