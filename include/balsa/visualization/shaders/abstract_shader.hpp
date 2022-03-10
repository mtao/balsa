#if !defined(BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP)
#define BALSA_VISUALIZATION_SHADERS_ABSTRACT_SHADER_HPP

#include <memory>
#include <limits>
#include "balsa/scene_graph/camera.hpp"

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
    static std::vector<char> compile_glsl(const std::string &glsl, ShaderType);
    // supports qfile
    static std::vector<char> compile_glsl_from_path(const std::string &path, ShaderType);
    virtual ~AbstractShader() {}
    virtual std::vector<char> vert_spirv() const { return {}; }
    virtual std::vector<char> frag_spirv() const { return {}; }
};
}// namespace balsa::visualization::shaders
#endif
