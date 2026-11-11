#include <catch2/catch_test_macros.hpp>

#include "balsa/visualization/colormap_list.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/shaders/embedded_sources.hpp"
#include "balsa/visualization/shaders/program_shader.hpp"
#include "balsa/visualization/shaders/shader_library.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

#include <array>
#include <string>

namespace shaders = balsa::visualization::shaders;

TEST_CASE("unsupported_shader_type_is_rejected", "[visualization][shaders]") {
    const shaders::AbstractShader shader;
    CHECK(
        shader
            .compile_glsl("#version 450\nvoid main() {}",
                          static_cast<shaders::AbstractShader::ShaderType>(-1))
            .empty());
}

TEST_CASE("shader_library_supports_extension", "[visualization][shaders]") {
    shaders::ShaderLibrary library;

    REQUIRE(library.add_shader("user/vertex", "vertex source").has_value());
    CHECK(library.contains("user/vertex"));
    REQUIRE(library.find("user/vertex"));
    CHECK(*library.find("user/vertex") == "vertex source");

    const auto duplicate = library.add_shader("user/vertex", "duplicate");
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error() == shaders::ShaderLibraryError::DuplicatePath);
    CHECK(*library.find("user/vertex") == "vertex source");
}

TEST_CASE("shader_library_overlays_builtins", "[visualization][shaders]") {
    auto library = shaders::ShaderLibrary::overlay(shaders::builtin_shader_library());
    const auto initial_revision = library.revision();

    REQUIRE(library.find("phong/vertex"));
    REQUIRE(library.add_shader("user/fragment", "user source").has_value());
    CHECK(library.revision() > initial_revision);

    const auto duplicate = library.add_shader("phong/vertex", "duplicate");
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error() == shaders::ShaderLibraryError::DuplicatePath);

    const auto old_source = library.find("phong/vertex");
    REQUIRE(library.override_shader("phong/vertex", "override source").has_value());
    CHECK(*library.find("phong/vertex") == "override source");
    CHECK(old_source->starts_with("#version"));

    const auto missing = library.override_shader("missing", "source");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == shaders::ShaderLibraryError::PathNotFound);
}

TEST_CASE("shader_library_shares_revisions_safely", "[visualization][shaders]") {
    shaders::ShaderLibrary parent;
    auto overlay = shaders::ShaderLibrary::overlay(parent);
    auto parent_handle = parent;
    auto overlay_handle = overlay;

    const auto initial_revision = overlay_handle.revision();
    REQUIRE(parent.add_shader("parent/source", "parent").has_value());
    CHECK(parent_handle.contains("parent/source"));
    CHECK(overlay_handle.contains("parent/source"));
    CHECK(overlay_handle.revision() > initial_revision);

    parent.make_read_only();
    const auto read_only_add = parent_handle.add_shader("blocked", "source");
    REQUIRE_FALSE(read_only_add.has_value());
    CHECK(read_only_add.error() == shaders::ShaderLibraryError::ReadOnly);

    CHECK(shaders::builtin_shader_library().is_read_only());
    auto builtin_handle = shaders::builtin_shader_library();
    const auto builtin_override = builtin_handle.override_shader("phong/vertex", "blocked");
    REQUIRE_FALSE(builtin_override.has_value());
    CHECK(builtin_override.error() == shaders::ShaderLibraryError::ReadOnly);
}

TEST_CASE("embedded_shader_sources_are_available", "[visualization][shaders]") {
    constexpr std::array paths{
        "flat/vertex",
        "flat/fragment",
        "examples/triangle/vertex",
        "examples/triangle/fragment",
        "phong/vertex",
        "phong/fragment",
        "unlit/vertex",
        "unlit/fragment",
        "gltf_metallic_roughness/vertex",
        "gltf_metallic_roughness/fragment",
        "image/vertex",
        "image/fragment",
    };
    for (const auto path : paths) {
        const auto source = shaders::builtin_shader_library().find(path);
        REQUIRE(source);
        CHECK(source->starts_with("#version"));
        CHECK(source->contains("void main"));
    }
    CHECK_FALSE(shaders::builtin_shader_library().find("missing"));
}

TEST_CASE("embedded_colormap_registry_is_consistent", "[visualization][shaders]") {
    const auto names = shaders::colormap_names();

    REQUIRE(names.size() == 104);
    for (const auto name : names) {
        const auto source = shaders::colormap_shader_source(name);
        REQUIRE(source.has_value());
        CHECK(source->contains("vec4 colormap(float x)"));
        CHECK(balsa::visualization::find_colormap_index(name) >= 0);
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            CHECK(names[i] != names[j]);
        }
    }

    CHECK_FALSE(shaders::colormap_shader_source("gnuplot").has_value());
    CHECK_FALSE(shaders::colormap_shader_source("missing").has_value());
    CHECK(balsa::visualization::find_colormap_index("missing") == -1);
}

TEST_CASE("embedded_shaders_compile", "[visualization][shaders]") {
    shaders::AbstractShader compiler;
    const auto image_vertex = shaders::builtin_shader_library().find("image/vertex");
    const auto image_fragment = shaders::builtin_shader_library().find("image/fragment");
    REQUIRE(image_vertex);
    REQUIRE(image_fragment);
    CHECK_FALSE(compiler.compile_glsl(
      std::string_view{*image_vertex},
      shaders::AbstractShader::ShaderType::Vertex).empty());
    CHECK_FALSE(compiler.compile_glsl(
      std::string_view{*image_fragment},
      shaders::AbstractShader::ShaderType::Fragment).empty());
}

TEST_CASE("phong_program_compiles_every_colormap", "[visualization][shaders][programs]") {
    const auto program = shaders::programs::phong();
    const auto shader = [&](std::string_view colormap) {
        return shaders::ProgramShader(
            program.value(),
            {.defines = {"SHADING_FLAT", "HAS_SCALARS", "COLORMAP"},
             .includes = {{"colormap.glsl", "colormaps/" + std::string(colormap)}}});
    };
    for (const auto name : shaders::colormap_names()) {
        INFO(name);
        CHECK_FALSE(shader(name).frag_spirv().empty());
    }
    // An unknown colormap has no shader source.
    CHECK(shader("missing").frag_spirv().empty());
}

TEST_CASE("shader_program_permutations_compile", "[visualization][shaders][programs]") {
    namespace programs = shaders::programs;
    const auto compiles = [](const shaders::ShaderProgram &program, shaders::ProgramVariant variant) {
        CAPTURE(program.id(), variant.defines);
        const shaders::ProgramShader shader(program.value(), std::move(variant));
        CHECK_FALSE(shader.vert_spirv().empty());
        CHECK_FALSE(shader.frag_spirv().empty());
    };
    // Every combination of a program's optional streams.
    const auto stream_defines = [](const shaders::ShaderProgram &program, unsigned mask) {
        std::vector<std::string> defines;
        unsigned bit = 0;
        for (const auto &stream : program.value().streams) {
            if (stream.define.empty()) continue;
            if ((mask >> bit++) & 1u) defines.push_back(stream.define);
        }
        return defines;
    };
    for (unsigned mask = 0; mask < 32; ++mask) {
        compiles(programs::gltf_metallic_roughness(),
                 {.defines = stream_defines(programs::gltf_metallic_roughness(),
                                            mask)});
    }
    for (const auto *shading : {"SHADING_FLAT", "SHADING_GOURAUD", "SHADING_PHONG", "SHADING_UNLIT"}) {
        for (unsigned mask = 0; mask < 4; ++mask) {
            auto defines = stream_defines(programs::phong(), mask);
            defines.push_back(shading);
            compiles(programs::phong(), {.defines = defines});
            defines.push_back("COLORMAP");
            compiles(programs::phong(),
                     {.defines = defines, .includes = {{"colormap.glsl", "colormaps/MATLAB_jet"}}});
        }
    }
    compiles(programs::unlit_lines(), {});
    compiles(programs::unlit_points(), {});
}

TEST_CASE("shader_program_missing_include_fails", "[visualization][shaders][programs]") {
    // COLORMAP without a colormap include cannot compile.
    const shaders::ProgramShader shader(shaders::programs::phong().value(),
                                        {.defines = {"SHADING_PHONG", "HAS_SCALARS", "COLORMAP"}});
    CHECK(shader.frag_spirv().empty());
}
