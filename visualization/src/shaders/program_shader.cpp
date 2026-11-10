#include "balsa/visualization/shaders/program_shader.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace balsa::visualization::shaders {

namespace {
    constexpr std::string_view colormap_prefix = "colormaps/";

    // Resolves #include names; owns the results until shaderc releases them.
    class Includer : public shaderc::CompileOptions::IncluderInterface {
      public:
        Includer(ShaderLibrary library, std::vector<std::pair<std::string, std::string>> aliases)
          : m_library(std::move(library)), m_aliases(std::move(aliases)) {}

        auto GetInclude(const char *requested, shaderc_include_type, const char *, size_t)
            -> shaderc_include_result * override {
            std::string name = requested;
            for (const auto &[alias, target] : m_aliases) {
                if (alias == name) name = target;
            }
            auto *result = new Result{.name = name};
            if (auto source = find(name)) {
                result->content = std::move(*source);
            } else {
                // shaderc reports an empty source name as a failed include.
                result->name.clear();
                result->content = "cannot find shader source \"" + name + "\"";
            }
            result->result = {result->name.c_str(), result->name.size(),
                              result->content.c_str(), result->content.size(), result};
            return &result->result;
        }

        void ReleaseInclude(shaderc_include_result *data) override {
            delete static_cast<Result *>(data->user_data);
        }

      private:
        struct Result {
            std::string name;
            std::string content{};
            shaderc_include_result result{};
        };

        auto find(std::string_view name) const -> std::optional<std::string> {
            if (name.starts_with(colormap_prefix)) {
                return colormap_shader_source(name.substr(colormap_prefix.size()))
                    .transform([](std::string_view source) { return std::string(source); });
            }
            if (const auto source = m_library.find(name)) return *source;
            return std::nullopt;
        }

        ShaderLibrary m_library;
        std::vector<std::pair<std::string, std::string>> m_aliases;
    };

    auto source(const ShaderLibrary &library, const std::string &name) -> std::string {
        const auto found = library.find(name);
        return found ? *found : std::string{};
    }
} // namespace

void ProgramShader::add_compile_options(shaderc::CompileOptions &opts) const {
    AbstractShader::add_compile_options(opts);
    for (const auto &define : _variant.defines) opts.AddMacroDefinition(define);
    opts.SetIncluder(std::make_unique<Includer>(_library, _variant.includes));
}

std::vector<uint32_t> ProgramShader::vert_spirv() const {
    return compile_glsl(source(_library, _program->vertex_shader), ShaderType::Vertex);
}

std::vector<uint32_t> ProgramShader::frag_spirv() const {
    return compile_glsl(source(_library, _program->fragment_shader), ShaderType::Fragment);
}

} // namespace balsa::visualization::shaders
