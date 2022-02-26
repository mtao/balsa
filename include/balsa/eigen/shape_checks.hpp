#if !defined(BALSA_EIGEN_SHAPE_CHECKS_HPP)
#define BALSA_EIGEN_SHAPE_CHECKS_HPP
#include <Eigen/Core>
#include "balsa/eigen/concepts/shape_types.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>

namespace balsa::eigen {

// These functions return in case asserts are disabled
template<int... Cs, typename MatType>
requires(concepts::ColCompatible<Cs, MatType> || ... || false) constexpr bool col_check_oneof(const MatType &N,
                                                                                              std::integer_sequence<int, Cs...>) {

    constexpr int cols = MatType::ColsAtCompileTime;
    if constexpr (cols == Eigen::Dynamic) {
        return ((N.cols() == Cs) || ... || false);
    }
    return true;
}

template<int... Rs, typename MatType>
requires(concepts::RowCompatible<Rs, MatType> || ... || false) constexpr bool row_check_oneof(const MatType &N,
                                                                                              std::integer_sequence<int, Rs...>) {
    constexpr int rows = MatType::RowsAtCompileTime;
    if constexpr (rows == Eigen::Dynamic) {
        return ((N.rows() == Rs) || ... || false);
    }
    return true;
}

template<int C, typename MatType>
requires concepts::ColCompatible<C, MatType> constexpr bool col_check(const MatType &N) {
    return col_check_oneof(N, std::integer_sequence<int, C>{});
}

template<int R, typename MatType>
requires concepts::RowCompatible<R, MatType> constexpr bool row_check(const MatType &N) {
    return row_check_oneof(N, std::integer_sequence<int, R>{});
}
template<int R, int C, typename MatType>
constexpr bool shape_check(const MatType &N) {
    return row_check<R>(N) && col_check<C>(N);
}

template<int C, typename MatType>
requires concepts::ColCompatible<C, MatType> constexpr void col_check_with_throw(const MatType &N) {
    if (!col_check<C>(N)) {
        throw std::invalid_argument(fmt::format("Col check: wrong size, got {} expected {}", N.cols(), C));
    }
}

template<int R, typename MatType>
requires concepts::RowCompatible<R, MatType> constexpr void row_check_with_throw(const MatType &N) {

    if (!row_check<R>(N)) {
        throw std::invalid_argument(fmt::format("Row check: wrong size, got {} expected {}", N.rows(), R));
    }
}
template<int R, int C, typename MatType>
constexpr void shape_check_with_throw(const MatType &N) {
    row_check_with_throw<R>(N);
    col_check_with_throw<C>(N);
}

template<int... C, typename MatType>
void col_check_with_throw(const MatType &N, std::integer_sequence<int, C...> Seq) {
    if (!col_check_oneof(N, Seq)) {
        std::string msg = fmt::format("Col check: wrong size, got {} expected one of {}", N.cols(), std::make_tuple(C...));
        throw std::invalid_argument(msg);
    }
}

template<int... R, typename MatType>
void row_check_with_throw(const MatType &N, std::integer_sequence<int, R...> Seq) {

    if (!row_check_oneof(N, Seq)) {
        std::string msg = fmt::format("Row check: wrong size, got {} expected {}", N.rows(), std::make_tuple(R...));
        throw std::invalid_argument(msg);
    }
}
template<int... R, int... C, typename MatType>
void shape_check_with_throw(const MatType &N, std::integer_sequence<int, R...> RSeq, std::integer_sequence<int, C...> CSeq) {
    row_check_with_throw(N, RSeq) && col_check_with_throw(N, CSeq);
}
}// namespace balsa::eigen
#endif
