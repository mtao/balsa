#pragma once
// The glTF structs carry balsa::json annotations, which need C++26
// reflection (GCC 16+ with -freflection); see balsa/json/annotations.hpp.
#include <balsa/json/annotations.hpp>
#include <balsa/json/raw_json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace balsa::visualization::scene_graph::gltf {

// glTF objects: properties are optional unless marked Required, arrays have
// minItems 1, and an absent property means its default. So unannotated
// members are omitted when empty.
inline constexpr auto object = json::members(json::omit_empty);

// JSON object with extension-specific objects.
using Extension = json::RawJson;
// Application-specific data.
using Extras = json::RawJson;

} // namespace balsa::visualization::scene_graph::gltf
