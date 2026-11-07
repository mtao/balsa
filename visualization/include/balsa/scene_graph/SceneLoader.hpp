#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace balsa::scene_graph {

class Object;

// Loads a scene file into an Object tree by extension.
// Loaders are assumed to be stateless copy-able true functions for now
class SceneLoader {
  public:
    using result_type = std::expected<std::unique_ptr<Object>, std::string>;
    using loader_function_type =
        std::copyable_function<result_type(const std::filesystem::path &) const>;

    SceneLoader();
    SceneLoader(const SceneLoader &other);
    SceneLoader(SceneLoader &&other) noexcept;
    auto operator=(const SceneLoader &other) -> SceneLoader &;
    auto operator=(SceneLoader &&other) noexcept -> SceneLoader &;
    ~SceneLoader();

    // Handles .obj, and .gltf / .glb when built with glTF support.
    static auto defaults() -> SceneLoader;

    // Makes `load` handle `extension` (with its leading '.', e.g. ".obj"),
    // replacing any existing loader.
    auto add(std::string_view extension, loader_function_type load) &
        -> SceneLoader &;
    auto add(std::string_view extension, loader_function_type load) &&
        -> SceneLoader &&;

    auto handles(std::string_view extension) const -> bool;
    // Fails if `path` does not exist or its extension has no loader.
    auto load(const std::filesystem::path &path) const -> result_type;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace balsa::scene_graph
