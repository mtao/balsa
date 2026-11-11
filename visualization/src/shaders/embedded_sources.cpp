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
constexpr unsigned char scene_include[] = {
#embed "scene.glsl"
};
constexpr unsigned char phong_material_include[] = {
#embed "phong_material.glsl"
};
constexpr unsigned char phong_vertex[] = {
#embed "phong.vert"
};
constexpr unsigned char phong_fragment[] = {
#embed "phong.frag"
};
constexpr unsigned char unlit_vertex[] = {
#embed "unlit.vert"
};
constexpr unsigned char unlit_fragment[] = {
#embed "unlit.frag"
};
constexpr unsigned char gltf_metallic_roughness_vertex[] = {
#embed "gltf_metallic_roughness.vert"
};
constexpr unsigned char gltf_metallic_roughness_fragment[] = {
#embed "gltf_metallic_roughness.frag"
};
constexpr unsigned char debug_view_include[] = {
#embed "debug_view.glsl"
};
constexpr unsigned char attribute_color_vertex[] = {
#embed "attribute_color.vert"
};
constexpr unsigned char attribute_color_fragment[] = {
#embed "attribute_color.frag"
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
        (void)library.add_shader("balsa/scene.glsl", std::string{as_string_view(scene_include)});
        (void)library.add_shader("balsa/phong_material.glsl",
                                 std::string{as_string_view(phong_material_include)});
        (void)library.add_shader("phong/vertex", std::string{as_string_view(phong_vertex)});
        (void)library.add_shader("phong/fragment", std::string{as_string_view(phong_fragment)});
        (void)library.add_shader("unlit/vertex", std::string{as_string_view(unlit_vertex)});
        (void)library.add_shader("unlit/fragment", std::string{as_string_view(unlit_fragment)});
        (void)library.add_shader(
            "gltf_metallic_roughness/vertex",
            std::string{as_string_view(gltf_metallic_roughness_vertex)});
        (void)library.add_shader(
            "gltf_metallic_roughness/fragment",
            std::string{as_string_view(gltf_metallic_roughness_fragment)});
        (void)library.add_shader("balsa/debug_view.glsl",
                                 std::string{as_string_view(debug_view_include)});
        (void)library.add_shader("attribute_color/vertex",
                                 std::string{as_string_view(attribute_color_vertex)});
        (void)library.add_shader("attribute_color/fragment",
                                 std::string{as_string_view(attribute_color_fragment)});
        (void)library.add_shader("image/vertex", std::string{as_string_view(image_vertex)});
        (void)library.add_shader("image/fragment", std::string{as_string_view(image_fragment)});
        library.make_read_only();
        return true;
    }();
    (void)initialized;
    return library;
}

}// namespace balsa::visualization::shaders
