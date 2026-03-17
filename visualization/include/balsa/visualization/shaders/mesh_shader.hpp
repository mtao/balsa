#if !defined(BALSA_VISUALIZATION_SHADERS_MESH_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_MESH_SHADER_HPP

#include <string>
#include <shaderc/shaderc.hpp>
#include "balsa/scene_graph/embedding_traits.hpp"
#include "balsa/visualization/shaders/shader.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::visualization::shaders {

// Uber-shader for mesh rendering.
//
// Compiles mesh.vert and mesh.frag with #define permutations driven by
// a MeshRenderState.  For the COLOR_SCALAR_FIELD path, the selected
// colormap GLSL function is prepended to the fragment source.
//
// This is a header-only template (like FlatShader) so that the
// embedding-dimension define is preserved.
template<scene_graph::concepts::embedding_traits ET>
class MeshShader : public Shader<ET> {
  public:
    explicit MeshShader(const vulkan::MeshRenderState &state)
      : _state(&state) {}

    void add_compile_options(shaderc::CompileOptions &opts) const override;
    std::vector<uint32_t> vert_spirv() const override;
    std::vector<uint32_t> frag_spirv() const override;

  private:
    const vulkan::MeshRenderState *_state;
};

// ── Implementation ───────────────────────────────────────────────────

template<scene_graph::concepts::embedding_traits ET>
void MeshShader<ET>::add_compile_options(shaderc::CompileOptions &opts) const {
    Shader<ET>::add_compile_options(opts);

    // Shading model
    switch (_state->shading) {
    case vulkan::ShadingModel::Flat:
        opts.AddMacroDefinition("SHADING_FLAT");
        break;
    case vulkan::ShadingModel::Gouraud:
        opts.AddMacroDefinition("SHADING_GOURAUD");
        break;
    case vulkan::ShadingModel::Phong:
        opts.AddMacroDefinition("SHADING_PHONG");
        break;
    }

    // Color source
    switch (_state->color_source) {
    case vulkan::ColorSource::Uniform:
        opts.AddMacroDefinition("COLOR_UNIFORM");
        break;
    case vulkan::ColorSource::PerVertex:
        opts.AddMacroDefinition("COLOR_PER_VERTEX");
        break;
    case vulkan::ColorSource::ScalarField:
        opts.AddMacroDefinition("COLOR_SCALAR_FIELD");
        break;
    }

    // Normal source
    if (_state->normal_source == vulkan::NormalSource::FromAttribute) {
        opts.AddMacroDefinition("HAS_NORMALS");
    }

    // Render mode — wireframe uses unlit wireframe color
    if (_state->render_mode == vulkan::RenderMode::Wireframe) {
        opts.AddMacroDefinition("RENDER_WIREFRAME");
    }
}

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> MeshShader<ET>::vert_spirv() const {
    const static std::string fname = ":/glsl/mesh.vert";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Vertex);
}

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> MeshShader<ET>::frag_spirv() const {
    // Read the base mesh fragment shader source.
    std::string frag_source = AbstractShader::read_path_to_string(":/glsl/mesh.frag");

    if (_state->color_source == vulkan::ColorSource::ScalarField) {
        // Load the colormap GLSL function from Qt resources.
        // The colormap shaders have NO #version directive — they are pure
        // function definitions exporting `vec4 colormap(float x)`.
        std::string colormap_path = ":/colormap-shaders/" + _state->colormap_name + ".frag";
        std::string colormap_source = AbstractShader::read_path_to_string(colormap_path);

        // Inject the colormap source after the `#version` line.
        // mesh.frag starts with `#version 460 core\n` — find the end of
        // that line and insert the colormap definitions right after it.
        auto version_end = frag_source.find('\n');
        if (version_end != std::string::npos) {
            frag_source.insert(version_end + 1,
                               "\n// ── Injected colormap ──\n" + colormap_source + "\n");
        }
    }

    return AbstractShader::compile_glsl(frag_source, AbstractShader::ShaderType::Fragment);
}

}// namespace balsa::visualization::shaders

#endif
