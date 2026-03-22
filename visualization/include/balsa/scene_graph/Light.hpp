#if !defined(BALSA_SCENE_GRAPH_LIGHT_HPP)
#define BALSA_SCENE_GRAPH_LIGHT_HPP

#include "AbstractFeature.hpp"
#include "types.hpp"

#include <cstdint>

namespace balsa::scene_graph {

// ── Light ───────────────────────────────────────────────────────────
//
// Feature that makes an Object act as a light source.
//
// Currently supports directional lights only (the most common case
// for mesh visualization).  The direction vector is specified in the
// Object's *local* coordinate space.  When the Light feature is
// attached to the Camera Object, the direction automatically tracks
// the camera orientation — giving "headlight" behaviour for free.
//
// The world-space direction is obtained by transforming `direction`
// through the owning Object's world_transform().  This is done by
// MeshScene::resolve_scene_lights() once per frame.
//
// Lighting parameters (ambient, specular, shininess) are stored here
// so that all meshes using scene lights share the same values.

class Light : public AbstractFeature {
  public:
    enum class Type : uint8_t {
        Directional,
        // Point and Spot reserved for future use.
    };

    Light() = default;

    // ── Light properties ────────────────────────────────────────────

    Type type = Type::Directional;

    // Direction in the owning Object's local space.
    // Default (0, 0, 1) means "forward" in camera space when
    // attached to the camera Object — i.e. a headlight shining
    // along the view direction.
    Vec3f direction{ 0.0f, 0.0f, 1.0f };

    // Lighting parameters (shared by all meshes using scene lights).
    float ambient_strength = 0.15f;
    float specular_strength = 0.5f;
    float shininess = 32.0f;

    // Light color (white by default).
    Vec3f color{ 1.0f, 1.0f, 1.0f };

    // Whether this light is active.
    bool enabled = true;
};

}// namespace balsa::scene_graph

#endif
