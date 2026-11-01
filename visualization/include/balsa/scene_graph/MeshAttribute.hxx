#pragma once
#include "MeshAttribute.hpp"

#include <cassert>

#include <zipper/DataArrayBase.hxx>
#include <zipper/expression/nullary/MDSpan.hpp>

namespace balsa::scene_graph {

template <AttributeValue T,
          typename Extents,
          typename Layout,
          typename Accessor>
    requires(Extents::rank() >= 1 && Extents::rank() <= 3)
MeshAttribute::MeshAttribute(
    std::string name,
    AttributeDomain domain,
    ::zipper::mdspan<T, Extents, Layout, Accessor> data,
    bool normalized)
  : name(std::move(name)), domain(domain), m_normalized(normalized) {
    assert(!normalized || is_normalizable(attribute_type_of<T>));
    using S = std::remove_cv_t<T>;
    constexpr auto R = Extents::rank();
    // Elements are (rows, columns): rank 1 is 1 x 1, rank 2 rows x 1.
    const std::size_t rows = R == 1 ? 1 : data.extent(0);
    const std::size_t columns = R == 3 ? data.extent(1) : 1;
    const std::size_t count = data.extent(R - 1);
    assert(columns >= 1 && rows * columns <= max_components);

    storage_type<S> values(rows * columns, count);
    using dst_span =
        ::zipper::mdspan<S, ::zipper::dextents<R>, ::zipper::storage::col_major>;
    using dst_t = ::zipper::expression::nullary::MDSpan<
        S, ::zipper::dextents<R>, ::zipper::storage::col_major,
        ::zipper::default_accessor_policy<S>>;
    using src_t =
        ::zipper::expression::nullary::MDSpan<T, Extents, Layout, Accessor>;
    ::zipper::DataArrayBase(dst_t(dst_span(values.data(), data.extents()))) =
        ::zipper::DataArrayBase(src_t(data)).template cast<S>();

    m_values = std::move(values);
    m_columns = static_cast<uint8_t>(columns);
}

} // namespace balsa::scene_graph
