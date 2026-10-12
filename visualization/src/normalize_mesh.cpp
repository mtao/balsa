#include "balsa/visualization/normalize_mesh.hpp"

#include <balsa/geometry/bounding_box.hpp>
#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/zipper/types.hpp>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/max_coeff.hpp>

namespace balsa::visualization {

auto normalize_to_unit_box(scene_graph::Object &object,
                           const scene_graph::MeshData &mesh_data) -> bool {
    if (!mesh_data.has_positions()) { return false; }
    const auto view = mesh_data.position_binding().view<3>();
    if (!view || view->extent(1) == 0) { return false; }
    // Opt into zipper for the math.
    const balsa::zipper::ColVectors<float, 3>::const_span_type positions(*view);
    const auto bbox = geometry::bounding_box(positions);

    // Largest extent across axes; guard degenerate meshes.
    float range = ::zipper::utils::maxCoeff(bbox.range());
    if (range < 1e-8f) { range = 1.0f; }

    // Object composes T * R * S, so scale is applied before translation:
    // world = s * x + t.  For world = s * (x - center), t = -s * center.
    const float s = 1.0f / range;
    object.set_translation(scene_graph::Vec3f(-s * bbox.center()));
    object.set_scale_factors(scene_graph::Vec3f(
      ::zipper::expression::nullary::Constant<float, 3>(s)));
    return true;
}

} // namespace balsa::visualization
