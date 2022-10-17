#if !defined(BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP

#include <memory>
#include <limits>
#include "balsa/scene_graph/camera.hpp"

namespace shaderc {
class CompileOptions;
}

// calls qrc which can't be used in a namespace
void balsa_visualization_shaders_initialize_resources();

namespace balsa::visualization::shaders {

class AbstractShader {
  public:
    enum class ShaderType {
        Vertex,
        Fragment

    };
    AbstractShader();
    std::vector<uint32_t> compile_glsl(const std::string &glsl, ShaderType) const;
    // supports qresource so can't use filesystem path
    std::vector<uint32_t> compile_glsl_from_path(const std::string &path, ShaderType) const;


    virtual void add_compile_options(shaderc::CompileOptions &) const {}
    virtual ~AbstractShader() {}
    virtual std::vector<uint32_t> vert_spirv() const { return {}; }
    virtual std::vector<uint32_t> frag_spirv() const { return {}; }

    static std::string read_path_to_string(const std::string& path);

};
}// namespace balsa::visualization::shaders
#endif
