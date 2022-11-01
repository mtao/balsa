#if !defined(BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP

#include <memory>
#include <limits>
#include "balsa/scene_graph/Camera.hpp"

namespace shaderc {
class CompileOptions;
}

// calls qrc which can't be used in a namespace
void balsa_visualization_shaders_initialize_resources();

namespace balsa::visualization::shaders {

class AbstractShader {
  public:
    enum class ShaderStage {
        Vertex,
        Fragment

    };
    struct SpirvShader {
        std::string name = "main";
        ShaderStage stage;
        std::vector<uint32_t> spirv_data;
    };
    AbstractShader();
    std::vector<uint32_t> compile_glsl(const std::string &glsl, ShaderStage) const;
    // supports qresource so can't use filesystem path
    std::vector<uint32_t> compile_glsl_from_path(const std::string &path, ShaderStage) const;


    virtual void add_compile_options(shaderc::CompileOptions &) const {}
    virtual ~AbstractShader() {}
    virtual std::vector<uint32_t> vert_spirv() const = 0;//{ return {}; }
    virtual std::vector<uint32_t> frag_spirv() const = 0;//{ return {}; }

    static std::string read_path_to_string(const std::string &path);

    virtual std::vector<SpirvShader> compile_spirv();
};
}// namespace balsa::visualization::shaders
#endif
