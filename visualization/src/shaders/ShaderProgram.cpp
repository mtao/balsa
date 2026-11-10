#include "balsa/visualization/shaders/ShaderProgram.hpp"

namespace balsa::visualization::shaders {

namespace {
    namespace roles = scene_graph::roles;
    using scene_graph::ColorSpace;

    auto position_stream() -> VertexStream {
        return {.role = roles::position(), .location = 0, .required = true};
    }

    // Built-ins are added in declaration order.
    struct Defaults {
        catalog::Catalog<ShaderProgramDefinition> catalog{"shader program"};
        ShaderProgram phong = catalog.add({
            .id = "balsa.phong",
            .label = "Phong",
            .vertex_shader = "phong/vertex",
            .fragment_shader = "phong/fragment",
            .streams = {position_stream(),
                        {.role = roles::normal(), .location = 1, .define = "HAS_NORMALS"},
                        {.role = roles::scalar(), .location = 2, .define = "HAS_SCALARS"}},
            .material_size = 64,
        });
        ShaderProgram pbr = catalog.add({
            .id = "balsa.pbr",
            .label = "PBR (metallic-roughness)",
            .vertex_shader = "pbr/vertex",
            .fragment_shader = "pbr/fragment",
            .streams = {position_stream(),
                        {.role = roles::normal(), .location = 1, .define = "HAS_NORMALS"},
                        {.role = roles::tangent(), .location = 2, .define = "HAS_TANGENTS"},
                        {.role = roles::texture_coordinates(0),
                         .location = 3,
                         .define = "HAS_TEXTURE_COORDINATES_0"},
                        {.role = roles::texture_coordinates(1),
                         .location = 4,
                         .define = "HAS_TEXTURE_COORDINATES_1"},
                        {.role = roles::color(), .location = 5, .define = "HAS_COLORS"}},
            .material_size = 80,
            .textures = {{"base_color", ColorSpace::Srgb},
                         {"metallic_roughness"},
                         {"normal", ColorSpace::Linear, TextureFallback::FlatNormal},
                         {"occlusion"},
                         {"emissive", ColorSpace::Srgb}},
        });
        ShaderProgram unlit_lines = catalog.add({
            .id = "balsa.unlit_lines",
            .label = "Unlit lines",
            .vertex_shader = "unlit/vertex",
            .fragment_shader = "unlit/fragment",
            .streams = {position_stream()},
            .material_size = 32,
        });
        ShaderProgram unlit_points = catalog.add({
            .id = "balsa.unlit_points",
            .label = "Unlit points",
            .vertex_shader = "unlit/vertex",
            .fragment_shader = "unlit/fragment",
            .streams = {position_stream()},
            .material_size = 32,
        });
    };

    auto defaults() -> Defaults & {
        static Defaults d;
        return d;
    }
} // namespace

auto shader_programs() -> catalog::Catalog<ShaderProgramDefinition> & {
    return defaults().catalog;
}

namespace programs {
    auto phong() -> ShaderProgram { return defaults().phong; }
    auto pbr() -> ShaderProgram { return defaults().pbr; }
    auto unlit_lines() -> ShaderProgram { return defaults().unlit_lines; }
    auto unlit_points() -> ShaderProgram { return defaults().unlit_points; }
} // namespace programs

} // namespace balsa::visualization::shaders
