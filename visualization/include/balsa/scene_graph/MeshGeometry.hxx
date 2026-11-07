#pragma once
#include "MeshGeometry.hpp"

#include <spdlog/spdlog.h>

#include <zipper/MatrixBase.hxx>

#include <concepts>
#include <type_traits>

namespace balsa::scene_graph {

template <std::size_t K, typename MDS>
auto MeshGeometry::to_element_indices(const MDS &indices)
    -> std::optional<element_indices<K>> {
    using E = typename MDS::extents_type;
    static_assert(std::integral<std::remove_cv_t<typename MDS::element_type>>,
                  "indices must be integral");

    // An element per row (N x K) or per column (K x N).  A static extent
    // of K decides (the row extent first); otherwise rows win if both
    // runtime extents are K.
    constexpr auto dyn = std::dynamic_extent;
    constexpr bool static_rows = E::static_extent(1) == K;
    constexpr bool static_columns = !static_rows && E::static_extent(0) == K;
    constexpr bool can_be_rows =
        static_rows || (!static_columns && E::static_extent(1) == dyn);
    constexpr bool can_be_columns =
        static_columns || (!static_rows && E::static_extent(0) == dyn);

    const bool rows = can_be_rows && indices.extent(1) == K;
    if (!rows && !(can_be_columns && indices.extent(0) == K)) {
        return std::nullopt;
    }

    const ::zipper::MatrixBase source(indices);
    element_indices<K> out(rows ? indices.extent(0) : indices.extent(1));
    ::zipper::MatrixBase dest(out.expression().as_mdspan());
    if constexpr (can_be_rows) {
        if (rows) {
            dest = source.transpose().template cast<uint32_t>();
            return out;
        }
    }
    if constexpr (can_be_columns) {
        dest = source.template cast<uint32_t>();
    }
    return out;
}

template <std::size_t K>
auto MeshGeometry::topology_view(std::string_view name) const
    -> element_indices_view<K> {
    if (const auto attr = find_attribute(name)) {
        if (auto view = attr->view<uint32_t, K>()) return *view;
    }
    return element_indices_view<K>(nullptr, 0);
}

template <std::size_t K>
auto MeshGeometry::set_topology(std::string_view name,
                                AttributeDomain domain,
                                element_indices_view<K> indices) -> void {
    insert_attribute(MeshAttribute(std::string(name), domain, indices));
}

template <AttributeValue T,
          typename Extents,
          typename Layout,
          typename Accessor>
    requires(Extents::rank() >= 1 && Extents::rank() <= 3)
auto MeshGeometry::add_attribute(std::string name,
                             AttributeDomain domain,
                             ::zipper::mdspan<T, Extents, Layout, Accessor> data,
                             AttributeOptions options) -> MeshAttributeHandle {
    if constexpr (Extents::rank() >= 2) {
        std::size_t components = data.extent(0);
        if constexpr (Extents::rank() == 3) { components *= data.extent(1); }
        if (components == 0 || components > MeshAttribute::max_components) {
            spdlog::warn("MeshGeometry: attribute '{}' has {} components, "
                         "expected 1..{}",
                         name,
                         components,
                         MeshAttribute::max_components);
            return {};
        }
    }
    if (is_reserved_name(name)) {
        spdlog::warn("MeshGeometry: attribute name '{}' is reserved", name);
        return {};
    }
    if (options.normalized && !is_normalizable(attribute_type_of<T>)) {
        spdlog::warn("MeshGeometry: attribute '{}' cannot be normalized "
                     "(only 8 / 16-bit integers can)",
                     name);
        return {};
    }
    const bool normalized = options.normalized;
    return add_user_attribute(
        MeshAttribute(std::move(name), domain, std::move(data), normalized),
        options);
}

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
auto MeshGeometry::add_attribute(std::string name,
                             AttributeDomain domain,
                             uint8_t component_count,
                             const R &data,
                             AttributeOptions options) -> MeshAttributeHandle {
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
            flat.size() / component_count),
        std::move(options));
}

} // namespace balsa::scene_graph
