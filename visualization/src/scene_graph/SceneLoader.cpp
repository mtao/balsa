#include "balsa/scene_graph/SceneLoader.hpp"

#include "balsa/scene_graph/MeshInstance.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/mesh_conversion.hpp"

#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/string/capitalization.hpp>

#if defined(BALSA_VISUALIZATION_GLTF)
#include "balsa/visualization/scene_graph/gltf/Document.hpp"
#include "balsa/visualization/scene_graph/gltf/scene_conversion.hpp"
#endif

#include <flat_map>
#include <format>

namespace balsa::scene_graph {

namespace {
    namespace ascii = balsa::string::ascii;

    auto load_obj(const std::filesystem::path &path) -> SceneLoader::result_type {
        const auto obj = geometry::triangle_mesh::read_objF(path);
        if (obj.position.vertices.cols() == 0) {
            return std::unexpected(
                std::format("no vertices in {}", path.string()));
        }
        auto object = std::make_unique<Object>(path.filename().string());
        object->emplace_feature<MeshInstance>(std::make_shared<MeshGeometry>(
            visualization::to_mesh_geometry(obj)));
        return object;
    }

#if defined(BALSA_VISUALIZATION_GLTF)
    auto load_gltf(const std::filesystem::path &path) -> SceneLoader::result_type {
        namespace gltf = balsa::visualization::scene_graph::gltf;
        auto object =
            gltf::load(path).and_then([](const gltf::Document &document) {
                return gltf::to_object(document);
            });
        if (!object) {
            return std::unexpected(std::move(object.error().message));
        }
        (*object)->name = path.filename().string();
        return std::move(*object);
    }
#endif
} // namespace

struct SceneLoader::Impl {
    std::flat_map<std::string, loader_function_type, ascii::IgnoreCaseLess>
        loaders;
};

SceneLoader::SceneLoader() : m_impl(std::make_unique<Impl>()) {}
SceneLoader::SceneLoader(const SceneLoader &other)
  : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
SceneLoader::SceneLoader(SceneLoader &&other) noexcept = default;
auto SceneLoader::operator=(const SceneLoader &other) -> SceneLoader & {
    if (this != &other) { m_impl = std::make_unique<Impl>(*other.m_impl); }
    return *this;
}
auto SceneLoader::operator=(SceneLoader &&other) noexcept
    -> SceneLoader & = default;
SceneLoader::~SceneLoader() = default;

auto SceneLoader::defaults() -> SceneLoader {
    SceneLoader loader;
    loader.add(".obj", load_obj);
#if defined(BALSA_VISUALIZATION_GLTF)
    loader.add(".gltf", load_gltf).add(".glb", load_gltf);
#endif
    return loader;
}

auto SceneLoader::add(std::string_view extension, loader_function_type load) &
    -> SceneLoader & {
    m_impl->loaders.insert_or_assign(std::string(extension), std::move(load));
    return *this;
}

auto SceneLoader::add(std::string_view extension, loader_function_type load) &&
    -> SceneLoader && {
    return std::move(add(extension, std::move(load)));
}

auto SceneLoader::handles(std::string_view extension) const -> bool {
    return m_impl->loaders.contains(extension);
}

auto SceneLoader::load(const std::filesystem::path &path) const -> result_type {
    if (!std::filesystem::exists(path)) {
        return std::unexpected(
            std::format("file not found: {}", path.string()));
    }
    const auto it = m_impl->loaders.find(path.extension().string());
    if (it == m_impl->loaders.end()) {
        return std::unexpected(
            std::format("unsupported file type: {}", path.string()));
    }
    return it->second(path);
}

} // namespace balsa::scene_graph
