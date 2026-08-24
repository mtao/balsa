#include <catch2/catch_test_macros.hpp>

#include "balsa/visualization/colormap_list.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/shaders/embedded_sources.hpp"
#include "balsa/visualization/shaders/shader_library.hpp"

#include <array>
#include <string>

namespace shaders = balsa::visualization::shaders;

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

    REQUIRE(library.find("mesh/vertex"));
    REQUIRE(library.add_shader("user/fragment", "user source").has_value());
    CHECK(library.revision() > initial_revision);

    const auto duplicate = library.add_shader("mesh/vertex", "duplicate");
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error() == shaders::ShaderLibraryError::DuplicatePath);

    const auto old_source = library.find("mesh/vertex");
    REQUIRE(library.override_shader("mesh/vertex", "override source").has_value());
    CHECK(*library.find("mesh/vertex") == "override source");
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
    const auto builtin_override = builtin_handle.override_shader("mesh/vertex", "blocked");
    REQUIRE_FALSE(builtin_override.has_value());
    CHECK(builtin_override.error() == shaders::ShaderLibraryError::ReadOnly);
}

TEST_CASE("embedded_shader_sources_are_available", "[visualization][shaders]") {
    constexpr std::array paths{
        "flat/vertex",
        "flat/fragment",
        "examples/triangle/vertex",
        "examples/triangle/fragment",
        "mesh/vertex",
        "mesh/fragment",
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
    const auto mesh_fragment = shaders::builtin_shader_library().find("mesh/fragment");
    REQUIRE(image_vertex);
    REQUIRE(image_fragment);
    REQUIRE(mesh_fragment);

    CHECK_FALSE(compiler.compile_glsl(
      std::string_view{*image_vertex},
      shaders::AbstractShader::ShaderType::Vertex).empty());
    CHECK_FALSE(compiler.compile_glsl(
      std::string_view{*image_fragment},
      shaders::AbstractShader::ShaderType::Fragment).empty());

    std::string fragment{*mesh_fragment};
    const auto version_end = fragment.find('\n');
    REQUIRE(version_end != std::string::npos);
    fragment.insert(version_end + 1, *shaders::colormap_shader_source("MATLAB_jet"));
    fragment.insert(version_end + 1, "\n#define COLOR_SCALAR_FIELD\n#define SHADING_FLAT\n");
    CHECK_FALSE(compiler.compile_glsl(
      fragment, shaders::AbstractShader::ShaderType::Fragment).empty());
}
