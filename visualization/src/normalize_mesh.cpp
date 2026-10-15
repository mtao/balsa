#include "balsa/visualization/normalize_mesh.hpp"

#include <balsa/geometry/bounding_box.hpp>
#include <balsa/scene_graph/BasicShading.hpp>
#include <zipper/MatrixBase.hxx>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/max_coeff.hpp>

namespace balsa::visualization {

auto unit_box_transform(const scene_graph::BasicShading &shading)
    -> std::optional<scene_graph::AffineTransformf> {
    const auto &positions = shading.position_binding();
    if (positions.component_count() != 3 || positions.size() == 0) {
        return std::nullopt;
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

    // Scale first, then translate: s * x + t = s * (x - center), so
    // t = -s * center.
    const float s = 1.0f / range;
    const scene_graph::Translation3f translation(
        scene_graph::Vec3f(-s * bbox.center()));
    const scene_graph::Scaling3f scaling{ scene_graph::Vec3f(
        ::zipper::expression::nullary::Constant<float, 3>(s)) };
    return translation * scaling;
}

} // namespace balsa::visualization
