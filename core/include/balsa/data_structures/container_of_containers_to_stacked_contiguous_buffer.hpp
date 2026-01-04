#if !defined(BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER)
#define BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER
#include <Eigen/Core>
#include <ranges>

#include "stacked_contiguous_buffer.hpp"
#include <range/v3/view/transform.hpp>
#include <range/v3/view/partial_sum.hpp>


namespace balsa::data_structures {


template<typename IndexType, typename ContainerType>
    requires requires(ContainerType t, typename ContainerType::value_type u, typename ContainerType::value_type::value_type v) {
        t.size();
        u.size();
        IndexType(v);
    }
auto container_of_containers_to_stacked_contiguous_buffer(const ContainerType &container) -> StackedContiguousBuffer<IndexType> {


    StackedContiguousBuffer<IndexType> ret;
    {
        ret._offsets.resize({ container.size() + 1 });
        ret._offsets(0) = 0;
        auto range = container | ranges::views::transform([](const auto &a) -> size_t {
                         return a.size();
                     })
                     | ranges::views::partial_sum | std::ranges::views::take(ret._offsets.rows() - 1);
        std::ranges::copy(range, ret._offsets.view().data() + 1);
    }

    {
        ret._buffer.resize(index_type(ret._offsets(ret._offsets.rows() - 1)));

        // TODO: could be take exactly?
        auto range = container | std::ranges::views::join | std::ranges::views::take(ret._buffer.rows());
        std::ranges::copy(range, ret._buffer.view().data());
    }
    return ret;
}

}// namespace balsa::data_structures
#endif
