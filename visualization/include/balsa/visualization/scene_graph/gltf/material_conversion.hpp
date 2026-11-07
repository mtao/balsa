#pragma once
#include "Document.hpp"
#include "LoadError.hpp"
#include "Sampler.hpp"

#include <balsa/scene_graph/Material.hpp>

#include <expected>
#include <memory>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// glTF's sampler; undefined filters are linear, with linear mipmapping.
auto to_sampler(const Sampler &sampler) -> balsa::scene_graph::Sampler;

// The document's materials, indexed like manifest().materials.  Each image
// is decoded once and shared, as are textures, between the materials using
// them.  Textures without a source (e.g. supplied only by an unsupported
// extension) are left out.
auto to_materials(const Document &document)
    -> std::expected<std::vector<std::shared_ptr<balsa::scene_graph::Material>>, LoadError>;

} // namespace balsa::visualization::scene_graph::gltf
