#pragma once
#include "balsa/eigen/types.hpp"
#include <range/v3/view/enumerate.hpp>
#include "balsa/eigen/container_size.hpp"
#include <bitset>
namespace balsa::eigen {

template<typename Derived>
auto stl2eigen(const Eigen::EigenBase<Derived> &vec) {
    return vec;
}
template<typename Derived>
auto &stl2eigen(const Eigen::MatrixBase<Derived> &vec) {
    return vec;
}
template<typename T, typename Allocator, typename = std::enable_if_t<std::is_scalar_v<T>>>
auto stl2eigen(const std::vector<T, Allocator> &vec) {
    return Eigen::Map<const VectorX<T>>(vec.data(), vec.size());
}
template<typename T, size_t D, typename = std::enable_if_t<std::is_scalar_v<T>>>
auto stl2eigen(const std::array<T, D> &vec) {
    return Eigen::Map<const Vector<T, D>>(vec.data());
}
template<typename T, typename Allocator, typename = std::enable_if_t<std::is_scalar_v<T>>>
auto stl2eigen(std::vector<T, Allocator> &vec) {
    return Eigen::Map<VectorX<T>>(vec.data(), vec.size());
}
template<typename T, size_t D, typename = std::enable_if_t<std::is_scalar_v<T>>>
auto stl2eigen(std::array<T, D> &vec) {
    return Eigen::Map<Vector<T, D>>(vec.data());
}
template<size_t D>
auto stl2eigen(const std::bitset<D> &bs) {
    Vector<bool, D> v;
    for (int j = 0; j < D; ++j) {
        v(j) = bs[j];
    }
    return v;
}


template<typename Container>
auto stl2eigen(const Container &vec) {

    constexpr static int CSize = container_size<Container>();
    using InnerType = typename Container::value_type;
    if constexpr (std::is_base_of_v<Eigen::MatrixBase<InnerType>, InnerType>) {//internal eigen matrix
        using T = typename InnerType::Scalar;
        constexpr int D = InnerType::RowsAtCompileTime;
        static_assert(D != Eigen::Dynamic, "To guarantee constant sizes we only support converting containers of constant size Eigen Vectors");
        Matrix<T, D, CSize> ret(D, vec.size());
        for (auto &&[i, arr] : ranges::views::enumerate(vec)) {
            ret.col(i) = arr;
        }
        return ret;
    } else if constexpr (std::is_scalar_v<InnerType>) {//scalar so vector
        using T = InnerType;
        Vector<T, CSize> ret(vec.size());
        std::copy(vec.begin(), vec.end(), ret.data());
        return ret;
    } else {//filled with another container
        constexpr int D = container_size<InnerType>();
        using T = typename InnerType::value_type;
        static_assert(D != Eigen::Dynamic, "To guarantee constant sizes we only support converting containers of constant size Eigen Vectors");

        ColVectors<T, D> ret(D, vec.size());
        for (auto &&[i, arr] : ranges::views::enumerate(vec)) {
            ret.col(i) = stl2eigen(arr);
        }

        return ret;
    }
}

}// namespace balsa::eigen
