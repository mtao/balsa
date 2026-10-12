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
    const auto &pos_bind = mesh_data.position_binding();
    const auto *fdata = static_cast<const float *>(pos_bind.raw_data());
    const std::size_t n = pos_bind.size();

    // The span views the buffer as tightly packed xyz (ColVectors is
    // column-major: (c, i) -> fdata[c + 3 * i]).
    if (fdata == nullptr || n == 0 || pos_bind.component_count() != 3) {
        return false;
    }
    const balsa::zipper::ColVectors<float, 3>::const_span_type positions(fdata, n);
    const auto bbox = geometry::bounding_box(positions);

    // Largest extent across axes; guard degenerate meshes.
    float range = ::zipper::utils::maxCoeff(bbox.range());
    if (range < 1e-8f) { range = 1.0f; }

    object.set_translation(scene_graph::Vec3f(-bbox.center()));
    object.set_scale_factors(scene_graph::Vec3f(
      ::zipper::expression::nullary::Constant<float, 3>(1.0f / range)));
    return true;
}

} // namespace balsa::visualization
