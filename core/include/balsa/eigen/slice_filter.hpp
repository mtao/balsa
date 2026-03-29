#if !defined(BALSA_EIGEN_SLICE_FILTER_HPP)
#define BALSA_EIGEN_SLICE_FILTER_HPP

#include "balsa/eigen/types.hpp"


namespace balsa::eigen {

// Filter rows or columns of a matrix by a boolean mask.
// When Row == true, selects rows where M(i) is true.
// When Row == false (default), selects columns where M(i) is true.
// Returns a dense matrix containing only the selected rows/columns.
template<bool Row = false, typename Derived>
auto slice_filter(const Eigen::MatrixBase<Derived> &A, const VectorX<bool> &M) {

    using Scalar = typename Derived::Scalar;
    constexpr static int RACT = Row ? Eigen::Dynamic : Derived::RowsAtCompileTime;
    constexpr static int CACT = Row ? Derived::ColsAtCompileTime : Eigen::Dynamic;

    int size = M.count();

    using Mat = Eigen::Matrix<Scalar, RACT, CACT>;
    Mat B(Row ? size : A.rows(), Row ? A.cols() : size);

    int c = 0;
    for (int i = 0; i < (Row ? A.rows() : A.cols()); ++i) {
        if (M(i)) {
            if constexpr (Row) {
                B.row(c++) = A.row(i);
            } else {
                B.col(c++) = A.col(i);
            }
        }
    }
    return B;
}

template<typename Derived>
auto slice_filter_row(const Eigen::MatrixBase<Derived> &A, const VectorX<bool> &M) {
    return slice_filter<true>(A, M);
}

template<typename Derived>
auto slice_filter_col(const Eigen::MatrixBase<Derived> &A, const VectorX<bool> &M) {
    return slice_filter<false>(A, M);
}

}// namespace balsa::eigen
#endif
