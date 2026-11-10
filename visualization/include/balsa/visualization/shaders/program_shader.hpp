#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <shaderc/shaderc.hpp>

#include "balsa/visualization/shaders/ShaderProgram.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/shaders/embedded_sources.hpp"
#include "balsa/visualization/shaders/shader_library.hpp"

namespace balsa::visualization::shaders {

// One permutation of a ShaderProgram: macro definitions, and include names
// resolved to other sources (e.g. "colormap.glsl" to "colormaps/MATLAB_jet").
struct ProgramVariant {
    std::vector<std::string> defines{};
    std::vector<std::pair<std::string, std::string>> includes{};

    auto operator==(const ProgramVariant &) const -> bool = default;
};

// Compiles a program's sources.  `#include "name"` resolves, in order, to
// the variant's includes, then "colormaps/<colormap name>", then the
// library's sources.
class ProgramShader : public AbstractShader {
  public:
    ProgramShader(const ShaderProgramDefinition &program,
                  ProgramVariant variant,
                  const ShaderLibrary &library = builtin_shader_library())
      : _program(&program), _variant(std::move(variant)), _library(library) {}

    void add_compile_options(shaderc::CompileOptions &opts) const override;
    std::vector<uint32_t> vert_spirv() const override;
    std::vector<uint32_t> frag_spirv() const override;

  private:
    const ShaderProgramDefinition *_program;
    ProgramVariant _variant;
    ShaderLibrary _library;
};

} // namespace balsa::visualization::shaders
