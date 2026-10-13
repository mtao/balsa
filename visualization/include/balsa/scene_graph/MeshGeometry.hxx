#pragma once
#include "MeshGeometry.hpp"

#include <spdlog/spdlog.h>

#include <concepts>
#include <cstring>
#include <type_traits>

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
auto MeshGeometry::add_attribute(std::string name,
                             AttributeDomain domain,
                             ::zipper::mdspan<T, Extents, Layout, Accessor> data)
    -> MeshAttributeHandle {
    if constexpr (Extents::rank() == 2) {
        if (data.extent(0) == 0
            || data.extent(0) > MeshAttribute::max_components) {
            spdlog::warn("MeshGeometry: attribute '{}' has {} components, "
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
auto MeshGeometry::add_attribute(std::string name,
                             AttributeDomain domain,
                             uint8_t component_count,
                             const R &data) -> MeshAttributeHandle {
    const std::span flat(data);
    if (component_count == 0 || flat.size() % component_count != 0) {
        spdlog::warn("MeshGeometry: attribute '{}' has {} values, not a multiple "
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
auto MeshGeometry::set_triangle_indices(
    ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool {
    std::vector<uint32_t> flat;
    if (!detail::flatten_element_indices<3>(indices, flat)) return false;
    assign_triangles(std::move(flat));
    return true;
}

template <typename T, typename Extents, typename Layout, typename Accessor>
    requires(Extents::rank() == 2)
auto MeshGeometry::set_edge_indices(
    ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool {
    std::vector<uint32_t> flat;
    if (!detail::flatten_element_indices<2>(indices, flat)) return false;
    assign_edges(std::move(flat));
    return true;
}

} // namespace balsa::scene_graph
