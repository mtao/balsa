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
enum class ShaderStage {
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Compute
};

struct SpirvShader {
    std::string name = "main";
    ShaderStage stage;
    std::vector<uint32_t> data;
};
struct DescriptorBinding {

    enum class Type {
        // Sampler,
        // CombinedImageSampler,
        // SampledImage,
        // StorageImage,
        // UniformTexelBuffer,
        // StorageTexelBuffer,
        UniformBuffer,
        // StorageBuffer,
        // UniformBufferDynamic,
        // StorageBufferDynamic,
        // InputAttachment,
    };

    uint32_t binding;
    Type type;
    uint32_t count;
    std::vector<ShaderStage> stages;
};

class AbstractShader {
  public:
    AbstractShader();
    std::vector<uint32_t> compile_glsl(const std::string &glsl, ShaderStage) const;
    // supports qresource so can't use filesystem path
    std::vector<uint32_t> compile_glsl_from_path(const std::string &path, ShaderStage) const;


    virtual void add_compile_options(shaderc::CompileOptions &) const {}
    virtual ~AbstractShader() {}
    virtual std::vector<ShaderStage> available_stages() const = 0;
    virtual std::vector<uint32_t> get_stage_spirv(ShaderStage stage) const = 0;
    virtual std::string get_stage_name(ShaderStage stage) const;

    virtual std::vector<DescriptorBinding> get_descriptor_bindings() const { return {}; }

    static std::string read_path_to_string(const std::string &path);

    virtual std::vector<SpirvShader> compile_spirv() const;
};

}// namespace balsa::visualization::shaders
#endif
