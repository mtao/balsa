#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "shaderc/status.h"
#include <QtCore/QTextStream>
#include <spdlog/spdlog.h>
#include <QtCore/QFile>
#include <iostream>
#include <shaderc/shaderc.hpp>

void balsa_visualization_shaders_initialize_resources() {
    Q_INIT_RESOURCE(glsl);
}
namespace balsa::visualization::shaders {

namespace {
    const std::string &get_shader_type_name(AbstractShader::ShaderType type) {
        const static std::string v0 = "Vertex";
        const static std::string v1 = "Fragment";
        switch (type) {
        case AbstractShader::ShaderType::Vertex:
            return v0;
        case AbstractShader::ShaderType::Fragment:
            return v1;
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
        }
    }
}// namespace


AbstractShader::AbstractShader() {
    balsa_visualization_shaders_initialize_resources();
}
std::vector<char> AbstractShader::compile_glsl(const std::string &glsl, ShaderType type) {

    const std::string &stage_name = get_shader_type_name(type);
    shaderc_shader_kind kind;
    switch (type) {
    case ShaderType::Vertex:
        kind = shaderc_glsl_default_vertex_shader;
        break;
    case ShaderType::Fragment:
        kind = shaderc_glsl_default_fragment_shader;
        break;
    }
    shaderc::Compiler compiler;
    shaderc::CompileOptions compile_options;
    auto result = compiler.CompileGlslToSpv(glsl.data(), glsl.size(), kind, stage_name.c_str(), "main", compile_options);
    auto status = result.GetCompilationStatus();
    if (status != shaderc_compilation_status_success) {
        const std::string &status_name = shaderc_compilation_status_name(status);
        const std::string &stage_name = get_shader_type_name(type);
        spdlog::error("shaderc failed to build {} shader due to a ({}) with {} errors and {} warnings:\n {}", stage_name, status_name, result.GetNumErrors(), result.GetNumWarnings(), result.GetErrorMessage());
    }

    if (result.GetNumWarnings() > 0) {
        spdlog::warn("shaderc caught {} warnings when compilng {}\n {}", result.GetNumWarnings(), stage_name, result.GetErrorMessage());
    }


    std::cout << glsl << std::endl;
    return {};
}

std::vector<char> AbstractShader::compile_glsl_from_path(const std::string &path, ShaderType type) {
    QFile file(path.c_str());
    if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
        spdlog::error("Was unable to read []", path);
        return {};
    }

    QTextStream _ts(&file);
    QString data = _ts.readAll();

    return compile_glsl(data.toStdString(), type);
}

}// namespace balsa::visualization::shaders
