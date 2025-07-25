#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_BUFFER_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_BUFFER_H

#include "balsa/data_structures/stacked_contiguous_buffer.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename IndexType = int>
struct PolygonBuffer : public data_structures::StackedContiguousBuffer<IndexType> {

    using Base = data_structures::StackedContiguousBuffer<IndexType>;
    using Base::Base;
    PolygonBuffer(Base &&b) : Base(std::move(b)) {}

    auto get_polygon(int index) const { return Base::get_span(index); }
    auto get_polygon_offsets(int index) const { return Base::get_span_offsets(index); }

    size_t polygon_count() const {
        return Base::span_count();
    }
    bool operator==(const PolygonBuffer &o) const { return Base::operator==(o); }
};

}// namespace balsa::geometry::polygon_mesh


#endif
