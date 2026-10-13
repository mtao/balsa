#include "balsa/scene_graph/RoleBinding.hpp"

#include <zipper/MatrixBase.hxx>
#include <zipper/VectorBase.hxx>

#include <type_traits>

namespace balsa::scene_graph {

namespace {
    // Per-element magnitudes of attr as a single-component attribute:
    // float stays float, other types become double.
    auto magnitude_of(const MeshAttribute &attr) -> MeshAttribute {
        return attr.visit([&](const auto &values) {
            using V = std::remove_const_t<
                typename std::decay_t<decltype(values)>::element_type>;
            using R = std::conditional_t<std::is_same_v<V, float>, float, double>;
            const auto norms = ::zipper::MatrixBase(values)
                                   .template cast<R>()
                                   .colwise()
                                   .norm()
                                   .eval();
            return MeshAttribute(attr.name + " (magnitude)",
                                 attr.domain,
                                 norms.expression().as_mdspan());
        });
    }
} // namespace

auto RoleBinding::clear() -> void {
    _attr = {};
    _component.reset();
    _magnitude.reset();
}

auto RoleBinding::whole(MeshAttributeHandle attr) -> RoleBinding {
    RoleBinding binding;
    binding._attr = std::move(attr);
    return binding;
}

auto RoleBinding::component(MeshAttributeHandle attr, uint8_t c) -> RoleBinding {
    RoleBinding binding;
    binding._attr = std::move(attr);
    binding._component = c;
    return binding;
}

auto RoleBinding::magnitude(MeshAttributeHandle attr) -> RoleBinding {
    RoleBinding binding;
    binding._magnitude =
        std::make_shared<const MeshAttribute>(magnitude_of(*attr));
    binding._attr = std::move(attr);
    binding._component = 0;
    return binding;
}

} // namespace balsa::scene_graph
