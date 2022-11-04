#include "balsa/visualization/shaders/AbstractShader.hpp"
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include "shaderc/status.h"
#include <QtCore/QTextStream>
#include <spdlog/spdlog.h>
#include <QtCore/QFile>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.hpp>

void balsa_visualization_shaders_initialize_resources() {
    Q_INIT_RESOURCE(glsl);
}
namespace balsa::visualization::shaders {

namespace {
    const std::string &get_shader_stage_name(ShaderStage stage) {
        const static std::string v0 = "Vertex";
        const static std::string v1 = "Fragment";
        const static std::string v2 = "TessellationControl";
        const static std::string v3 = "TessellationEvaluation";
        const static std::string v4 = "Geometry";
        const static std::string v5 = "Compute";
        const static std::string v_ = "Unknown";
        switch (stage) {
        case ShaderStage::Vertex:
            return v0;
        case ShaderStage::Fragment:
            return v1;
        case ShaderStage::TessellationControl:
            return v2;
        case ShaderStage::TessellationEvaluation:
            return v3;
        case ShaderStage::Geometry:
            return v4;
        case ShaderStage::Compute:
            return v5;
        default:
            return v_;
        }
    }
    const std::string &shaderc_compilation_status_name(shaderc_compilation_status status) {
        const static std::string e0 = "Success";
        const static std::string e1 = "Invalid Stage";
        const static std::string e2 = "Compilation Error";
        const static std::string e3 = "Internal Error";
        const static std::string e4 = "Null Result Object";
        const static std::string e5 = "Invalid Assembly";
        const static std::string e6 = "Validation Error";
        const static std::string e7 = "Transformation Error";
        const static std::string e8 = "Configuration Error";
        const static std::string v_ = "Unknown Error";
        switch (status) {
        case shaderc_compilation_status_success:
            return e0;
        case shaderc_compilation_status_invalid_stage:
            return e1;// error stage deduction
        case shaderc_compilation_status_compilation_error:
            return e2;
        case shaderc_compilation_status_internal_error:
            return e3;// unexpected failure
        case shaderc_compilation_status_null_result_object:
            return e4;
        case shaderc_compilation_status_invalid_assembly:
            return e5;
        case shaderc_compilation_status_validation_error:
            return e6;
        case shaderc_compilation_status_transformation_error:
            return e7;
        case shaderc_compilation_status_configuration_error:
            return e8;
        default:
            return v_;
        }
    }
}// namespace


AbstractShader::AbstractShader() {
    balsa_visualization_shaders_initialize_resources();
}

std::vector<uint32_t> AbstractShader::compile_glsl(const std::string &glsl, ShaderStage stage) const {

    const std::string &stage_name = get_shader_stage_name(stage);
    shaderc_shader_kind kind;
    switch (stage) {
    case ShaderStage::Vertex:
        kind = shaderc_vertex_shader;
        break;
    case ShaderStage::Fragment:
        kind = shaderc_fragment_shader;
        break;
    case ShaderStage::TessellationControl:
        kind = shaderc_tess_control_shader;
        break;
    case ShaderStage::TessellationEvaluation:
        kind = shaderc_tess_evaluation_shader;
        break;
    case ShaderStage::Geometry:
        kind = shaderc_geometry_shader;
        break;
    case ShaderStage::Compute:
        kind = shaderc_compute_shader;
        break;
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions compile_options;
    add_compile_options(compile_options);
    auto result = compiler.CompileGlslToSpv(glsl.data(), glsl.size(), kind, stage_name.c_str(), "main", compile_options);
    auto status = result.GetCompilationStatus();
    if (status != shaderc_compilation_status_success) {
        const std::string &status_name = shaderc_compilation_status_name(status);
        const std::string &stage_name = get_shader_stage_name(stage);
        spdlog::error("shaderc failed to build {} shader due to a ({}) with {} errors and {} warnings:\n {}", stage_name, status_name, result.GetNumErrors(), result.GetNumWarnings(), result.GetErrorMessage());
        return {};
    }

    if (result.GetNumWarnings() > 0) {
        spdlog::warn("shaderc caught {} warnings when compilng {}\n {}", result.GetNumWarnings(), stage_name, result.GetErrorMessage());
    }

    std::vector<uint32_t> ret{ result.begin(), result.end() };


    return ret;
}
std::string AbstractShader::read_path_to_string(const std::string &path) {
    QFile file(path.c_str());
    if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
        spdlog::error("Was unable to read []", path);
        return {};
    }

    QTextStream _ts(&file);
    QString data = _ts.readAll();
    return data.toStdString();
}

std::vector<uint32_t> AbstractShader::compile_glsl_from_path(const std::string &path, ShaderStage stage) const {

    return compile_glsl(read_path_to_string(path), stage);
}

// namespace {
//     vk::ShaderModule make_shader_module(const vk::Device &device, const std::vector<uint32_t> &spirv) {
//         VkShaderModuleCreateInfo createInfo{
//             .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
//             .codeSize = sizeof(uint32_t) * spirv.size(),
//             .pCode = spirv.data()
//         };
//         vk::ShaderModule module;
//         module = device.createShaderModule(createInfo, nullptr, &module);
//         return module;
//     }
// }// namespace
//
// void AbstractShader::make_shader(const vk::Device &device) {
//     auto vs_spirv = vert_spirv();
//     auto fs_spirv = frag_spirv();
//
//     auto vs_module = make_shader_module(device, vs_spirv);
//     auto fs_module = make_shader_module(device, fs_spirv);
// }
std::string AbstractShader::get_stage_name(ShaderStage) const {
    return "main";
}
std::vector<SpirvShader> AbstractShader::compile_spirv() const {
    auto stages = available_stages();
    return stages | ranges::views::transform([&](ShaderStage stage) -> SpirvShader {
               SpirvShader shader;
               shader.name = get_stage_name(stage);
               shader.stage = stage;
               shader.data = get_stage_spirv(stage);
               return shader;
           })
           | ranges::to<std::vector>;
}

}// namespace balsa::visualization::shaders
