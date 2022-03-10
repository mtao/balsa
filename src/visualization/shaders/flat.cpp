
#include "balsa/visualization/shaders/flat.hpp"
#include <spdlog/spdlog.h>
#include <iostream>

namespace balsa::visualization::shaders {

FlatShader::FlatShader() {
}

std::vector<char> FlatShader::vert_spirv() const {
    const static std::string fname = ":/glsl/flat.vert";
    return AbstractShader::compile_glsl_from_path(fname, ShaderType::Vertex);
}
std::vector<char> FlatShader::frag_spirv() const {
    const static std::string fname = ":/glsl/flat.frag";
    return AbstractShader::compile_glsl_from_path(fname, ShaderType::Fragment);
}
}// namespace balsa::visualization::shaders
