#pragma once
#include "balsa/scene_graph/types.hpp"

namespace balsa::scene_graph {

// ── Material Properties ─────────────────────────────────────────────
//
// Per-mesh material response to scene lighting.  The light source
// itself (direction, color) lives on the scene's Light feature;
// these fields control how the mesh reacts to that light.

struct MaterialProperties {
    float ambient_strength = 0.15f;
    float diffuse_strength = 1.0f;
    float specular_strength = 0.5f;
    float shininess = 32.0f;
};

}// namespace balsa::scene_graph
