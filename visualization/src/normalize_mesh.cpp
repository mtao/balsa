#include "balsa/visualization/normalize_mesh.hpp"

#include <balsa/geometry/bounding_box.hpp>
#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <zipper/MatrixBase.hxx>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/max_coeff.hpp>

namespace balsa::visualization {

auto normalize_to_unit_box(scene_graph::Object &object,
                           const scene_graph::MeshData &mesh_data) -> bool {
    if (!mesh_data.has_positions()) { return false; }
    const auto &positions = mesh_data.position_binding();
    if (positions.component_count() != 3 || positions.size() == 0) {
        return false;
    }
    // Positions may be stored in any attribute type; compute in float.
    const auto bbox = positions.visit([&](const auto &values) {
        using T = std::remove_const_t<
            typename std::decay_t<decltype(values)>::element_type>;
        const ::zipper::MatrixBase P(*positions.view<T, 3>());
        return geometry::bounding_box(P.template cast<float>());
    });

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
