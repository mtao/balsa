#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// The name of the node’s TRS property to animate, or the "weights" of the Morph
// Targets it instantiates. For the "translation" property, the values that are
// provided by the sampler are the translation along the X, Y, and Z axes. For
// the "rotation" property, the values are a quaternion in the order (x, y, z,
// w), where w is the scalar. For the "scale" property, the values are the
// scaling factors along the X, Y, and Z axes.
enum class AnimationPath {
    translation,
    rotation,
    scale,
    weights,
};

// Interpolation algorithm.
enum class AnimationInterpolation {
    LINEAR,
    STEP,
    CUBICSPLINE,
};

// The descriptor of the animated property.
struct [[=object]] AnimationChannelTarget {
    // The index of the node to animate. When undefined, the animated object MAY
    // be defined by an extension.
    std::optional<int64_t> node;
    // The name of the node’s TRS property to animate, or the "weights" of the
    // Morph Targets it instantiates. For the "translation" property, the values
    // that are provided by the sampler are the translation along the X, Y, and
    // Z axes. For the "rotation" property, the values are a quaternion in the
    // order (x, y, z, w), where w is the scalar. For the "scale" property, the
    // values are the scaling factors along the X, Y, and Z axes.
    [[=json::required]] AnimationPath path;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// An animation channel combines an animation sampler with a target property
// being animated.
struct [[=object]] AnimationChannel {
    // The index of a sampler in this animation used to compute the value for
    // the target.
    [[=json::required]] int64_t sampler;
    // The descriptor of the animated property.
    [[=json::required]] AnimationChannelTarget target;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// An animation sampler combines timestamps with a sequence of output values and
// defines an interpolation algorithm.
struct [[=object]] AnimationSampler {
    // The index of an accessor containing keyframe timestamps.
    [[=json::required]] int64_t input;
    // Interpolation algorithm.
    [[=json::skip_default]] AnimationInterpolation interpolation = AnimationInterpolation::LINEAR;
    // The index of an accessor, containing keyframe output values.
    [[=json::required]] int64_t output;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// A keyframe animation.
struct [[=object]] Animation {
    // An array of animation channels. An animation channel combines an
    // animation sampler with a target property being animated. Different
    // channels of the same animation MUST NOT have the same targets.
    [[=json::required]] std::vector<AnimationChannel> channels;
    // An array of animation samplers. An animation sampler combines timestamps
    // with a sequence of output values and defines an interpolation algorithm.
    [[=json::required]] std::vector<AnimationSampler> samplers;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const AnimationPath &v) -> void;
auto from_json(const nlohmann::json &j, AnimationPath &v) -> void;
auto to_json(nlohmann::json &j, const AnimationInterpolation &v) -> void;
auto from_json(const nlohmann::json &j, AnimationInterpolation &v) -> void;
auto to_json(nlohmann::json &j, const AnimationChannelTarget &v) -> void;
auto from_json(const nlohmann::json &j, AnimationChannelTarget &v) -> void;
auto to_json(nlohmann::json &j, const AnimationChannel &v) -> void;
auto from_json(const nlohmann::json &j, AnimationChannel &v) -> void;
auto to_json(nlohmann::json &j, const AnimationSampler &v) -> void;
auto from_json(const nlohmann::json &j, AnimationSampler &v) -> void;
auto to_json(nlohmann::json &j, const Animation &v) -> void;
auto from_json(const nlohmann::json &j, Animation &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
