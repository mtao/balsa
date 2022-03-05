#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_BUFFER_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_BUFFER_H

#include "balsa/eigen/types.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename IndexType = int>
struct PolygonBuffer {


    auto get_polygon(int index) const;
    auto get_polygon_offsets(int index) const;

    size_t polygon_count() const {
        return _offsets.size() - 1;
    }
    balsa::eigen::VectorX<IndexType> _buffer;
    balsa::eigen::VectorX<int> _offsets;
};

template<typename IndexType>
auto PolygonBuffer<IndexType>::get_polygon_offsets(int index) const {
    return _offsets.segment<2>(index);
}

template<typename IndexType>
auto PolygonBuffer<IndexType>::get_polygon(int index) const {
    auto offsets = get_polygon_offsets(index);
    return _buffer.segment(offsets(0), offsets(1) - offsets(0));
}
}// namespace balsa::geometry::polygon_mesh


#endif
