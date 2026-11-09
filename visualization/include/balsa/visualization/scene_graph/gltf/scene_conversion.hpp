#pragma once
#include "Document.hpp"
#include "LoadError.hpp"

#include <balsa/scene_graph/Object.hpp>

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>

namespace balsa::visualization::gltf {

// A glTF scene as a scene-graph Object tree: one Object per node, with
// its mesh primitives as MeshInstances (no shading). Defaults to the
// manifest's default scene.
auto to_object(const Document &document,
               std::optional<std::size_t> scene = std::nullopt)
    -> std::expected<std::unique_ptr<balsa::scene_graph::Object>, LoadError>;

} // namespace balsa::visualization::gltf
