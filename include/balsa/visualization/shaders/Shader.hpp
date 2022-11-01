#if !defined(BALSA_VISUALIZATION_SHADERS_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_SHADER_HPP

#include <memory>
#include <limits>
#include "balsa/scene_graph/EmbeddingTraits.hpp"
#include "balsa/visualization/shaders/AbstractShader.hpp"
#include <shaderc/shaderc.hpp>

namespace balsa::visualization::shaders {

template<scene_graph::concepts::EmbeddingTraits ET>
class Shader : public AbstractShader {
  public:
    virtual void add_compile_options(shaderc::CompileOptions &opts) const override;
};

template<scene_graph::concepts::EmbeddingTraits ET>
void Shader<ET>::add_compile_options(shaderc::CompileOptions &opts) const {
    AbstractShader::add_compile_options(opts);
    if constexpr (ET::embedding_dimension == 2) {
        opts.AddMacroDefinition("TWO_DIMENSIONS");
    } else if constexpr (ET::embedding_dimension == 3) {
        opts.AddMacroDefinition("THREE_DIMENSIONS");
    }
}
}// namespace balsa::visualization::shaders
#endif
