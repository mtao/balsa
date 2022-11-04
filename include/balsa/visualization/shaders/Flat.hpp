#if !defined(BALSA_VISUALIZATION_SHADERS_FLAT_HPP)
#define BALSA_VISUALIZATION_SHADERS_FLAT_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/EmbeddingTraits.hpp"
#include "balsa/visualization/shaders/Shader.hpp"

namespace balsa::visualization::shaders {

template<scene_graph::concepts::EmbeddingTraits ET>
class FlatShader : public Shader<ET> {
  public:
    FlatShader() {}
    std::vector<DescriptorBinding> get_descriptor_bindings() const override;
    std::vector<uint32_t> vert_spirv() const override final;
    std::vector<uint32_t> frag_spirv() const override final;
};

template<scene_graph::concepts::EmbeddingTraits ET>
std::vector<uint32_t> FlatShader<ET>::vert_spirv() const {
    const static std::string fname = ":/glsl/flat.vert";
    return AbstractShader::compile_glsl_from_path(fname, ShaderStage::Vertex);
}
template<scene_graph::concepts::EmbeddingTraits ET>
std::vector<uint32_t> FlatShader<ET>::frag_spirv() const {
    const static std::string fname = ":/glsl/flat.frag";
    return AbstractShader::compile_glsl_from_path(fname, ShaderStage::Fragment);
}
template<scene_graph::concepts::EmbeddingTraits ET>
std::vector<DescriptorBinding> FlatShader<ET>::get_descriptor_bindings() const {
    return {};
}
}// namespace balsa::visualization::shaders
#endif
