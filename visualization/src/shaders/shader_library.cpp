#include "balsa/visualization/shaders/shader_library.hpp"

namespace balsa::visualization::shaders {

struct ShaderLibrary::State {
    std::shared_ptr<State> parent;
    std::map<std::string, source_type, std::less<>> sources;
    std::uint64_t revision = 0;
    bool read_only = false;
};

ShaderLibrary::ShaderLibrary() : _state(std::make_shared<State>()) {}

auto ShaderLibrary::overlay(const ShaderLibrary &parent) -> ShaderLibrary {
    auto state = std::make_shared<State>();
    state->parent = parent._state;
    return ShaderLibrary{std::move(state)};
}

auto ShaderLibrary::add_shader(std::string path, std::string source)
  -> std::expected<void, ShaderLibraryError> {
    if (_state->read_only) {
        return std::unexpected(ShaderLibraryError::ReadOnly);
    }
    if (contains(path)) {
        return std::unexpected(ShaderLibraryError::DuplicatePath);
    }
    const auto [_, inserted] = _state->sources.emplace(
      std::move(path), std::make_shared<const std::string>(std::move(source)));
    if (!inserted) {
        return std::unexpected(ShaderLibraryError::DuplicatePath);
    }
    ++_state->revision;
    return {};
}

auto ShaderLibrary::override_shader(std::string path, std::string source)
  -> std::expected<void, ShaderLibraryError> {
    if (_state->read_only) {
        return std::unexpected(ShaderLibraryError::ReadOnly);
    }
    if (!contains(path)) {
        return std::unexpected(ShaderLibraryError::PathNotFound);
    }
    _state->sources.insert_or_assign(
      std::move(path), std::make_shared<const std::string>(std::move(source)));
    ++_state->revision;
    return {};
}

auto ShaderLibrary::find(std::string_view path) const -> source_type {
    for (auto state = _state; state; state = state->parent) {
        const auto it = state->sources.find(path);
        if (it != state->sources.end()) {
            return it->second;
        }
    }
    return nullptr;
}

auto ShaderLibrary::contains(std::string_view path) const -> bool {
    return static_cast<bool>(find(path));
}

auto ShaderLibrary::revision() const -> std::uint64_t {
    std::uint64_t revision = _state->revision;
    for (auto parent = _state->parent; parent; parent = parent->parent) {
        revision += parent->revision;
    }
    return revision;
}

auto ShaderLibrary::make_read_only() -> void {
    _state->read_only = true;
}

auto ShaderLibrary::is_read_only() const -> bool {
    return _state->read_only;
}

}// namespace balsa::visualization::shaders
