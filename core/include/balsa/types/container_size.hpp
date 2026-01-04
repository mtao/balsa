#if !defined(BALSA_TYPES_CONTAINER_SIZE_HPP)
#define BALSA_TYPES_CONTAINER_SIZE_HPP

#include "has_tuple_size.hpp"

namespace balsa::types {

template<typename T>
constexpr size_t container_size(const T &container) {
    if constexpr (has_tuple_size<T>) {
        return std::tuple_size<T>();
    } else {
        return container.size();
    }
}
#endif
