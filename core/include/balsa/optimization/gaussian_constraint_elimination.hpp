#if !defined(BALSA_OPTIMIZATION_GAUSSIAN_CONSTRAINT_ELIMINATION_HPP)
#define BALSA_OPTIMIZATION_GAUSSIAN_CONSTRAINT_ELIMINATION_HPP
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <type_traits>
#include "balsa/eigen/concepts/matrix_types.hpp"

namespace balsa::eigen {

// Say we want to solve Ax = b given Cx = d
// This function eliminates the constraints via Gaussian elimination
//
// NOTE: This is an incomplete stub. The loop bodies in
// gaussian_constraint_elimination_inline are not yet implemented.
template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
auto gaussian_constraint_elimination(const ConstraintMat &C, const Mat &A) -> decltype(A.eval());


template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
void gaussian_constraint_elimination_inline(ConstraintMat &C, Mat &A) {
    static_assert(std::is_same_v<typename ConstraintMat::Scalar, typename Mat::Scalar>);
    using Scalar = typename ConstraintMat::Scalar;

    // for each row eliminate the constraint
    for (int constraint_row_index = 0; constraint_row_index < C.rows(); ++constraint_row_index) {
        // find the pivot
        Eigen::SparseVector<Scalar> constraint_row = C.row(constraint_row_index);

        for (typename ConstraintMat::InnerIterator row_it(C, constraint_row_index); row_it; ++row_it) {
            // TODO: implement pivot selection and row elimination
        }

        // TODO: eliminate other terms
    }
}

template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
auto gaussian_constraint_elimination(const ConstraintMat &C_, const Mat &A_) {

    auto C = C_.eval();
    auto A = A_.eval();

    gaussian_constraint_elimination_inline(C, A);
    return A;
}
}// namespace balsa::eigen

#endif
