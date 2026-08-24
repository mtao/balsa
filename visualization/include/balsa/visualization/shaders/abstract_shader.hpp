#if !defined(BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP

#include <cstdint>
#include <string_view>
#include <vector>
// Camera is not needed by AbstractShader.

namespace shaderc {
class CompileOptions;
}

namespace balsa::visualization::shaders {

class AbstractShader {
  public:
    enum class ShaderType {
        Vertex,
        Fragment

    };
    AbstractShader() = default;
    auto compile_glsl(std::string_view glsl, ShaderType type) const -> std::vector<uint32_t>;


    virtual void add_compile_options(shaderc::CompileOptions &) const {}
    virtual ~AbstractShader() = default;
    virtual std::vector<uint32_t> vert_spirv() const { return {}; }
    virtual std::vector<uint32_t> frag_spirv() const { return {}; }
};
}// namespace balsa::visualization::shaders
#endif
