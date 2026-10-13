#pragma once
#include "MeshData.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <array>
#include <concepts>
#include <cstring>
#include <type_traits>

#include <zipper/MatrixBase.hxx>
#include <zipper/VectorBase.hxx>
#include <zipper/expression/nullary/MDSpan.hpp>

namespace balsa::scene_graph {

namespace detail {

    // Flatten a rank-2 mdspan of element indices (N x K or K x N, any
    // layout) into element-major order: K indices per element.
    // Returns false if neither extent equals K.
    template <std::size_t K, typename MDS>
    auto flatten_element_indices(const MDS &m, std::vector<uint32_t> &out)
        -> bool {
        using E = typename MDS::extents_type;
        using L = typename MDS::layout_type;
        using V = std::remove_cv_t<typename MDS::element_type>;
        static_assert(std::integral<V>, "indices must be integral");

        bool rows = false;
        if constexpr (E::static_extent(1) == K) {
            rows = true;
        } else if constexpr (E::static_extent(0) == K) {
            rows = false;
        } else {
            if (m.extent(1) == K) {
                rows = true;
            } else if (m.extent(0) == K) {
                rows = false;
            } else {
                return false;
            }
        }

        const std::size_t n = rows ? m.extent(0) : m.extent(1);
        out.resize(n * K);

        // Fast path: memory is already element-major uint32.
        constexpr bool u32 = std::is_same_v<V, uint32_t>;
        constexpr bool right =
            std::is_same_v<L, ::zipper::storage::layout_right>;
        constexpr bool left = std::is_same_v<L, ::zipper::storage::layout_left>;
        if constexpr (u32 && (right || left)) {
            if (rows == right) {
                if (n > 0) {
                    std::memcpy(
                        out.data(), m.data_handle(), n * K * sizeof(uint32_t));
                }
                return true;
            }
        }

        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t k = 0; k < K; ++k) {
                out[i * K + k] =
                    static_cast<uint32_t>(rows ? m[i, k] : m[k, i]);
            }
        }
        return true;
    }

} // namespace detail

template <AttributeValue T,
          typename Extents,
          typename Layout,
          typename Accessor>
    requires(Extents::rank() == 1 || Extents::rank() == 2)
MeshAttribute::MeshAttribute(
    std::string name,
    AttributeDomain domain,
    ::zipper::mdspan<T, Extents, Layout, Accessor> data)
  : name(std::move(name)), domain(domain) {
    using S = detail::attribute_storage_t<std::remove_cv_t<T>>;
    using source_t =
        ::zipper::expression::nullary::MDSpan<T, Extents, Layout, Accessor>;

    if constexpr (Extents::rank() == 1) {
        storage_type<S> values(std::size_t{1}, data.extent(0));
        typename balsa::zipper::VectorX<S>::span_type dst{
            std::span<S>(values.data(), data.extent(0))};
        dst = ::zipper::VectorBase<source_t>(source_t(data)).template cast<S>();
        m_values = std::move(values);
    } else {
        assert(data.extent(0) <= max_components);
        storage_type<S> values(data.extent(0), data.extent(1));
        values = ::zipper::DataArrayBase<source_t>(source_t(data))
                     .template cast<S>();
        m_values = std::move(values);
    }
}

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

template <AttributeValue T,
          typename Extents,
          typename Layout,
          typename Accessor>
    requires(Extents::rank() == 1 || Extents::rank() == 2)
auto MeshData::add_attribute(std::string name,
                             AttributeDomain domain,
                             ::zipper::mdspan<T, Extents, Layout, Accessor> data)
    -> MeshAttributeHandle {
    if constexpr (Extents::rank() == 2) {
        if (data.extent(0) == 0
            || data.extent(0) > MeshAttribute::max_components) {
            spdlog::warn("MeshData: attribute '{}' has {} components, "
                         "expected 1..{}",
                         name,
                         data.extent(0),
                         MeshAttribute::max_components);
            return {};
        }
    }
    return insert_attribute(
        MeshAttribute(std::move(name), domain, std::move(data)));
}

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
auto MeshData::add_attribute(std::string name,
                             AttributeDomain domain,
                             uint8_t component_count,
                             const R &data) -> MeshAttributeHandle {
    const std::span flat(data);
    if (component_count == 0 || flat.size() % component_count != 0) {
        spdlog::warn("MeshData: attribute '{}' has {} values, not a multiple "
                     "of {} components",
                     name,
                     flat.size(),
                     component_count);
        return {};
    }
    using E = std::remove_reference_t<decltype(*flat.data())>;
    return add_attribute(
        std::move(name),
        domain,
        ::zipper::mdspan<E, ::zipper::dextents<2>, ::zipper::storage::col_major>(
            flat.data(), std::size_t{component_count},
            flat.size() / component_count));
}

template <typename T, typename Extents, typename Layout, typename Accessor>
    requires(Extents::rank() == 2)
auto MeshData::set_triangle_indices(
    ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool {
    std::vector<uint32_t> flat;
    if (!detail::flatten_element_indices<3>(indices, flat)) return false;
    assign_triangles(std::move(flat));
    return true;
}

template <typename T, typename Extents, typename Layout, typename Accessor>
    requires(Extents::rank() == 2)
auto MeshData::set_edge_indices(
    ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool {
    std::vector<uint32_t> flat;
    if (!detail::flatten_element_indices<2>(indices, flat)) return false;
    assign_edges(std::move(flat));
    return true;
}

} // namespace balsa::scene_graph
