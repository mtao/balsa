#if !defined(BALSA_GEOMETRY_DATA_STRUCTURES_STACKED_CONTIGUOUS_BUFFER_H)
#define BALSA_GEOMETRY_DATA_STRUCTURES_STACKED_CONTIGUOUS_BUFFER_H

#include "balsa/eigen/types.hpp"
#include <tuple>


namespace balsa::data_structures {
template<typename IndexType = int>
struct StackedContiguousBuffer {


    auto get_span(int index) const;
    auto get_span_offsets(int index) const;

    size_t span_count() const {
        return _offsets.size() - 1;
    }
    balsa::eigen::VectorX<IndexType> _buffer;
    balsa::eigen::VectorX<int> _offsets;

    bool operator==(const StackedContiguousBuffer &o) const;
};

template<typename IndexType>
auto StackedContiguousBuffer<IndexType>::get_span_offsets(int index) const {
    return _offsets.segment<2>(index);
}

template<typename IndexType>
auto StackedContiguousBuffer<IndexType>::get_span(int index) const {
    auto offsets = get_span_offsets(index);
    return _buffer.segment(offsets(0), offsets(1) - offsets(0));
}

template<typename IndexType>
bool StackedContiguousBuffer<IndexType>::operator==(const StackedContiguousBuffer &o) const {
    return _offsets == o._offsets && o._buffer == _buffer;
}
}// namespace balsa::data_structures


#endif
