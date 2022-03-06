
#if !defined(BALSA_GEOMETRY_POLYGON_MESH_PLCURVE_BUFFER_H)
#define BALSA_GEOMETRY_POLYGON_MESH_PLCURVE_BUFFER_H

#include "balsa/data_structures/stacked_contiguous_buffer.hpp"
#include <tuple>
#include <vector>

namespace balsa::geometry::polygon_mesh {
template<typename IndexType = int>
struct PLCurveBuffer : public data_structures::StackedContiguousBuffer<IndexType> {

    using Base = data_structures::StackedContiguousBuffer<IndexType>;
    using Base::Base;
    PLCurveBuffer(Base &&b, bool is_default_closed = false) : Base(std::move(b)), is_closed(curve_count(), is_default_closed) {}
    PLCurveBuffer(Base &&b, std::vector<bool> closedness) : Base(std::move(b)), is_closed(std::move(closedness)) {
    }

    auto get_curve(int index) const { return Base::get_span(index); }
    auto get_curve_offsets(int index) const { return Base::get_span_offsets(index); }

    size_t curve_count() const {
        return Base::span_count();
    }
    bool operator==(const PLCurveBuffer &o) const { return Base::operator==(o); }

    std::vector<bool> is_closed;
};

}// namespace balsa::geometry::polygon_mesh


#endif
