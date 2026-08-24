#pragma once

#include <expected>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace balsa::visualization::shaders {

enum class ShaderLibraryError {
    DuplicatePath,
    PathNotFound,
    ReadOnly,
};

class ShaderLibrary {
  public:
    using source_type = std::shared_ptr<const std::string>;

    ShaderLibrary();
    static auto overlay(const ShaderLibrary &parent) -> ShaderLibrary;

    // Not thread-safe. Pipelines detect revisions and rebuild on demand.
    auto add_shader(std::string path, std::string source)
      -> std::expected<void, ShaderLibraryError>;
    auto override_shader(std::string path, std::string source)
      -> std::expected<void, ShaderLibraryError>;
    auto find(std::string_view path) const -> source_type;
    auto contains(std::string_view path) const -> bool;
    auto revision() const -> std::uint64_t;
    auto make_read_only() -> void;
    auto is_read_only() const -> bool;

  private:
    struct State;
    explicit ShaderLibrary(std::shared_ptr<State> state) : _state(std::move(state)) {}
    std::shared_ptr<State> _state;
};

auto builtin_shader_library() -> const ShaderLibrary &;

}// namespace balsa::visualization::shaders
