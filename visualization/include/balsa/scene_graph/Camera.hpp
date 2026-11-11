#pragma once
#include "AbstractFeature.hpp"
#include "types.hpp"

namespace balsa::scene_graph {

// ── Camera ──────────────────────────────────────────────────────────
//
// Feature that gives an Object the ability to act as a camera.
//
// The camera's position and orientation in the world are determined by
// its owning Object's world_transform().  The view matrix is the
// inverse of that transform.
//
// Projection can be either perspective or orthographic.  Parameters
// are cached so that update_aspect() can recompute the projection
// without requiring the caller to re-specify fov, near, far.

class Camera : public AbstractFeature {
  public:
    enum class Projection { Perspective,
                            Orthographic };

    // Which axis the perspective field of view applies to.
    //   Vertical:    fov is the vertical angle (glTF / OpenGL convention).
    //   ShorterSide: fov spans the window's shorter side, so content that
    //                fits keeps fitting in both portrait and landscape.
    enum class FovAxis { Vertical,
                         ShorterSide };

    Camera() = default;

    // ── Projection setup ────────────────────────────────────────────

    // Set a perspective projection.
    // fov_y:  field-of-view in radians along fov_axis() (vertical by default)
    // aspect: width / height
    void set_perspective(float fov_y, float aspect, float near, float far);

    // Set an orthographic projection.
    void set_orthographic(float left, float right, float bottom, float top, float near, float far);

    void set_fov_axis(FovAxis axis);

    // Update the aspect ratio, keeping existing fov/near/far.
    // Only meaningful for perspective projection.
    void update_aspect(float aspect);

    // ── Derived matrices ────────────────────────────────────────────

    // View matrix = inverse(object().world_transform()).
    Mat4f view_matrix() const;

    // The current projection matrix.
    const Mat4f &projection_matrix() const { return _projection; }

    // ── Accessors ───────────────────────────────────────────────────

    Projection projection_type() const { return _projection_type; }
    // The fov as given to set_perspective (along fov_axis()).
    float fov_y() const { return _fov_y; }
    FovAxis fov_axis() const { return _fov_axis; }
    // The actual vertical field of view after applying fov_axis().
    float vertical_fov() const;
    float aspect() const { return _aspect; }
    float near_plane() const { return _near; }
    float far_plane() const { return _far; }

  private:
    void recompute_perspective();

    Projection _projection_type = Projection::Perspective;
    Mat4f _projection;// default-initialized (identity)

    // Cached perspective parameters.
    float _fov_y = 0.7854f;// ~45 degrees
    FovAxis _fov_axis = FovAxis::Vertical;
    float _aspect = 1.0f;
    float _near = 0.01f;
    float _far = 100.0f;
};

}// namespace balsa::scene_graph
