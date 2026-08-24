#include <catch2/catch_test_macros.hpp>

#include "balsa/visualization/colormap_list.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/shaders/embedded_sources.hpp"

#include <array>
#include <string>

namespace shaders = balsa::visualization::shaders;

TEST_CASE("embedded_shader_sources_are_available", "[visualization][shaders]") {
    constexpr std::array embedded_shaders{
        shaders::EmbeddedShader::FlatVertex,
        shaders::EmbeddedShader::FlatFragment,
        shaders::EmbeddedShader::TriangleVertex,
        shaders::EmbeddedShader::TriangleFragment,
        shaders::EmbeddedShader::MeshVertex,
        shaders::EmbeddedShader::MeshFragment,
        shaders::EmbeddedShader::ImageVertex,
        shaders::EmbeddedShader::ImageFragment,
    };

    for (const auto shader : embedded_shaders) {
        const auto source = shaders::embedded_shader_source(shader);
        CHECK_FALSE(source.empty());
        CHECK(source.starts_with("#version"));
        CHECK(source.contains("void main"));
    }
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

    CHECK_FALSE(compiler.compile_glsl(
      shaders::embedded_shader_source(shaders::EmbeddedShader::ImageVertex),
      shaders::AbstractShader::ShaderType::Vertex).empty());
    CHECK_FALSE(compiler.compile_glsl(
      shaders::embedded_shader_source(shaders::EmbeddedShader::ImageFragment),
      shaders::AbstractShader::ShaderType::Fragment).empty());

    std::string fragment{shaders::embedded_shader_source(shaders::EmbeddedShader::MeshFragment)};
    const auto version_end = fragment.find('\n');
    REQUIRE(version_end != std::string::npos);
    fragment.insert(version_end + 1, *shaders::colormap_shader_source("MATLAB_jet"));
    fragment.insert(version_end + 1, "\n#define COLOR_SCALAR_FIELD\n#define SHADING_FLAT\n");
    CHECK_FALSE(compiler.compile_glsl(
      fragment, shaders::AbstractShader::ShaderType::Fragment).empty());
}
