#include "balsa/visualization/normalize_mesh.hpp"

#include <balsa/scene_graph/AttributeSemantic.hpp>
#include <balsa/scene_graph/MeshAttribute.hpp>
#include <balsa/scene_graph/MeshInstance.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <zipper/MatrixBase.hxx>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/max_coeff.hpp>

#include <bitset>

namespace balsa::visualization {

namespace {
    using Box = geometry::BoundingBox<float, 3>;

    auto positions_box(const scene_graph::MeshAttribute &positions)
        -> std::optional<Box> {
        if (positions.component_count() != 3 || positions.count() == 0) {
            return std::nullopt;
        }
        // Positions may be stored in any attribute type; compute in float.
        return positions.visit([&](const auto &values) {
            using T = std::remove_const_t<
                typename std::decay_t<decltype(values)>::element_type>;
            const ::zipper::MatrixBase P(*positions.view<T, 3>());
            return Box(P.template cast<float>());
        });
    }

    // Expands `box` by each mesh's box below `object`; `to_root` maps object
    // space to root space.
    void expand(Box &box,
                const scene_graph::Object &object,
                const scene_graph::AffineTransformf &to_root) {
        if (const auto *instance = object.find_feature<scene_graph::MeshInstance>();
            instance && instance->geometry()) {
            if (const auto positions =
                    instance->geometry()->attribute_for(scene_graph::semantics::position())) {
                if (const auto mesh_box = positions_box(*positions)) {
                    for (unsigned c = 0; c < 8; ++c) {
                        const auto corner =
                            (to_root * mesh_box->corner(std::bitset<3>(c)).homogeneous())
                                .eval();
                        box.expand(corner.head<3>().eval());
                    }
                }
            }
        }
        for (const auto &child : object.children()) {
            expand(box, *child, (to_root * child->local_transform()).eval());
        }
    }
} // namespace

auto unit_box_transform(const Box &box)
    -> std::optional<scene_graph::AffineTransformf> {
    if (box.is_empty()) { return std::nullopt; }

    // Largest extent across axes; guard degenerate boxes.
    float range = ::zipper::utils::maxCoeff(box.range());
    if (range < 1e-8f) { range = 1.0f; }

    // Scale first, then translate: s * x + t = s * (x - center), so
    // t = -s * center.
    const float s = 1.0f / range;
    const scene_graph::Translation3f translation(
        scene_graph::Vec3f(-s * box.center()));
    const scene_graph::Scaling3f scaling{ scene_graph::Vec3f(
        ::zipper::expression::nullary::Constant<float, 3>(s)) };
    return translation * scaling;
}

auto unit_box_transform(const scene_graph::MeshAttribute &positions)
    -> std::optional<scene_graph::AffineTransformf> {
    return positions_box(positions).and_then(
        [](const Box &box) { return unit_box_transform(box); });
}

auto local_bounding_box(const scene_graph::Object &root) -> Box {
    Box box;
    expand(box, root, scene_graph::AffineTransformf{});
    return box;
}

auto world_bounding_box(const scene_graph::Object &root) -> Box {
    Box box;
    expand(box, root, root.world_transform());
    return box;
}

} // namespace balsa::visualization
