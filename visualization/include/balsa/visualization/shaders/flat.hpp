#if !defined(BALSA_VISUALIZATION_SHADERS_FLAT_HPP)
#define BALSA_VISUALIZATION_SHADERS_FLAT_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/embedding_traits.hpp"
#include "balsa/visualization/shaders/shader.hpp"

namespace balsa::visualization::shaders {

template<scene_graph::concepts::embedding_traits ET>
class FlatShader : public Shader<ET> {
  public:
    FlatShader() {}
    std::vector<uint32_t> vert_spirv() const override final;
    std::vector<uint32_t> frag_spirv() const override final;
};

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> FlatShader<ET>::vert_spirv() const {
    const static std::string fname = ":/glsl/flat.vert";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Vertex);
}
template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> FlatShader<ET>::frag_spirv() const {
    const static std::string fname = ":/glsl/flat.frag";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Fragment);
}
}// namespace balsa::visualization::shaders
#endif
