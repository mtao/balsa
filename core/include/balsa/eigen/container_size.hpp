#if !defined(BALSA_EIGEN_CONTAINER_SIZE_HPP)
#define BALSA_EIGEN_CONTAINER_SIZE_HPP
#include "../types/has_tuple_size.hpp"
#include <Eigen/Core>

namespace balsa::eigen {
// outputs the size of a STL container like we would use for an eigen type. In particular, for static size objects it returns their size and for dynamic size ones it outputs dynamic
template<typename T>
constexpr int container_size() {
    if constexpr (types::has_tuple_size<T>) {
        return std::tuple_size<T>();
    } else {
        return Eigen::Dynamic;
    }
}
template<typename T>
constexpr int container_size(T &&) {
    return container_size<T>();
}
}// namespace balsa::eigen
#endif
