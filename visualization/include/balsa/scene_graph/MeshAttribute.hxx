#pragma once
#include "MeshAttribute.hpp"

#include <cassert>

#include <zipper/DataArrayBase.hxx>
#include <zipper/VectorBase.hxx>
#include <zipper/expression/nullary/MDSpan.hpp>

namespace balsa::scene_graph {

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

} // namespace balsa::scene_graph
