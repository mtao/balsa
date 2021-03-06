#if !defined(BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER)
#define BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER
#include <Eigen/Core>
#include <range/v3/view/take_exactly.hpp>

#include "stacked_contiguous_buffer.hpp"
#include <concepts>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/partial_sum.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/algorithm/copy.hpp>


namespace balsa::data_structures {


template<typename IndexType, typename ContainerType>
requires requires(ContainerType t, typename ContainerType::value_type u, typename ContainerType::value_type::value_type v) {
    t.size();
    u.size();
    IndexType(v);
}
StackedContiguousBuffer<IndexType> container_of_containers_to_stacked_contiguous_buffer(const ContainerType &container) {


    StackedContiguousBuffer<IndexType> ret;
    {
        ret._offsets.resize(container.size() + 1);
        ret._offsets(0) = 0;
        auto range = container | ranges::views::transform([](const auto &a) {
                         return a.size();
                     })
                     | ranges::views::partial_sum | ranges::views::take_exactly(ret._offsets.size() - 1);
        ranges::copy(range, ret._offsets.data() + 1);
    }

    {
        ret._buffer.resize(ret._offsets(Eigen::last));

        auto range = container | ranges::views::join | ranges::views::take_exactly(ret._buffer.size());
        ranges::copy(range, ret._buffer.data());
    }
    return ret;
}

}// namespace balsa::geometry::data_structures
#endif
