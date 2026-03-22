#if !defined(BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER)
#define BALSA_GEOMETRY_DATA_STRUCTURES_CONTAINER_OF_CONTAINERS_TO_STACKED_CONTIGUOUS_BUFFER
#include <ranges>
#include <numeric>

#include "stacked_contiguous_buffer.hpp"


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
        auto sizes = container | std::views::transform([](const auto &a) -> size_t {
                         return a.size();
                     });
        auto sizes_vec = sizes | std::ranges::to<std::vector>();
        std::partial_sum(sizes_vec.begin(), sizes_vec.end(), ret._offsets.expression().data() + 1);
    }

    {
        ret._buffer.resize(index_type(ret._offsets(ret._offsets.rows() - 1)));

        // TODO: could be take exactly?
        auto range = container | std::ranges::views::join | std::ranges::views::take(ret._buffer.rows());
        std::ranges::copy(range, ret._buffer.expression().data());
    }
    return ret;
}

}// namespace balsa::data_structures
#endif
