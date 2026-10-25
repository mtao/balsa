#pragma once
#include "Extension.hpp"
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// An orthographic camera containing properties to create an orthographic
// projection matrix.
struct [[=object]] CameraOrthographic {
    // The floating-point horizontal magnification of the view. This value MUST
    // NOT be equal to zero. This value SHOULD NOT be negative.
    [[=json::required]] double xmag;
    // The floating-point vertical magnification of the view. This value MUST
    // NOT be equal to zero. This value SHOULD NOT be negative.
    [[=json::required]] double ymag;
    // The floating-point distance to the far clipping plane. This value MUST
    // NOT be equal to zero. zfar MUST be greater than znear.
    [[=json::required]] double zfar;
    // The floating-point distance to the near clipping plane.
    [[=json::required]] double znear;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// A perspective camera containing properties to create a perspective projection
// matrix.
struct [[=object]] CameraPerspective {
    // The floating-point aspect ratio of the field of view.
    std::optional<double> aspectRatio;
    // The floating-point vertical field of view in radians. This value SHOULD
    // be less than π.
    [[=json::required]] double yfov;
    // The floating-point distance to the far clipping plane.
    std::optional<double> zfar;
    // The floating-point distance to the near clipping plane.
    [[=json::required]] double znear;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// Specifies if the camera uses a perspective or orthographic projection.
enum class CameraType {
    perspective,
    orthographic,
};

// A camera’s projection. A node MAY reference a camera to apply a transform to
// place the camera in the scene.
struct [[=object]] Camera {
    // An orthographic camera containing properties to create an orthographic
    // projection matrix. This property MUST NOT be defined when perspective is
    // defined.
    [[=json::manual_write]] std::optional<CameraOrthographic> orthographic;
    // A perspective camera containing properties to create a perspective
    // projection matrix. This property MUST NOT be defined when orthographic is
    // defined.
    [[=json::manual_write]] std::optional<CameraPerspective> perspective;
    // Specifies if the camera uses a perspective or orthographic projection.
    [[=json::required]] CameraType type;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const CameraType &v) -> void;
auto from_json(const nlohmann::json &j, CameraType &v) -> void;
auto to_json(nlohmann::json &j, const CameraOrthographic &v) -> void;
auto from_json(const nlohmann::json &j, CameraOrthographic &v) -> void;
auto to_json(nlohmann::json &j, const CameraPerspective &v) -> void;
auto from_json(const nlohmann::json &j, CameraPerspective &v) -> void;
auto to_json(nlohmann::json &j, const Camera &v) -> void;
auto from_json(const nlohmann::json &j, Camera &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
