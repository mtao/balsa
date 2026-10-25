#pragma once
#include "Accessor.hpp"
#include "Animation.hpp"
#include "Asset.hpp"
#include "Buffer.hpp"
#include "BufferView.hpp"
#include "Camera.hpp"
#include "Extension.hpp"
#include "Image.hpp"
#include "Material.hpp"
#include "Mesh.hpp"
#include "Node.hpp"
#include "Sampler.hpp"
#include "Scene.hpp"
#include "Skin.hpp"
#include "Texture.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// The root object for a glTF asset.
struct [[=object]] glTF {
    // Names of glTF extensions used in this asset.
    std::vector<std::string> extensionsUsed;
    // Names of glTF extensions required to properly load this asset.
    std::vector<std::string> extensionsRequired;
    // An array of accessors.
    std::vector<Accessor> accessors;
    // An array of keyframe animations.
    std::vector<Animation> animations;
    // Metadata about the glTF asset.
    [[=json::required]] Asset asset;
    // An array of buffers.
    std::vector<Buffer> buffers;
    // An array of bufferViews.
    std::vector<BufferView> bufferViews;
    // An array of cameras.
    std::vector<Camera> cameras;
    // An array of images.
    std::vector<Image> images;
    // An array of materials.
    std::vector<Material> materials;
    // An array of meshes.
    std::vector<Mesh> meshes;
    // An array of nodes.
    std::vector<Node> nodes;
    // An array of samplers.
    std::vector<Sampler> samplers;
    // The index of the default scene.
    std::optional<int64_t> scene;
    // An array of scenes.
    std::vector<Scene> scenes;
    // An array of skins.
    std::vector<Skin> skins;
    // An array of textures.
    std::vector<Texture> textures;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const glTF &v) -> void;
auto from_json(const nlohmann::json &j, glTF &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
