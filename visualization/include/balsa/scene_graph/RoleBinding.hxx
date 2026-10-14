#pragma once
#include "RoleBinding.hpp"

#include <array>

namespace balsa::scene_graph {

template <typename T, std::size_t D>
auto RoleBinding::view() const -> std::expected<view_type<T, D>, ViewError> {
    if (!is_bound()) {
        return std::unexpected(ViewError{ ViewError::Reason::Unbound });
    }
    const auto values = source().view<T>();
    if (!values) return std::unexpected(values.error());

    const std::size_t stored_components = values->extent(0);
    const std::size_t components = component_count();
    if constexpr (D != std::dynamic_extent) {
        if (components != D) {
            return std::unexpected(
                ViewError{ ViewError::Reason::WrongComponentCount,
                           type(),
                           static_cast<uint8_t>(components) });
        }
    }
    using extents_t = typename view_type<T, D>::extents_type;
    using mapping_t = typename view_type<T, D>::mapping_type;
    const auto extents = [&] {
        if constexpr (D == std::dynamic_extent) {
            return extents_t(components, size());
        } else {
            return extents_t(size());
        }
    }();
    // Column-major: next component is adjacent, next element is
    // stored_components further on.
    const std::array<std::size_t, 2> strides{ 1, stored_components };
    return view_type<T, D>(values->data_handle() + _component.value_or(0),
                           mapping_t(extents, strides));
}

template <typename F>
auto RoleBinding::visit(F &&f) const -> decltype(auto) {
    return source().visit([&](const auto &values) -> decltype(auto) {
        using T = std::remove_const_t<
            typename std::decay_t<decltype(values)>::element_type>;
        return f(*view<T>());
    });
}

} // namespace balsa::scene_graph
