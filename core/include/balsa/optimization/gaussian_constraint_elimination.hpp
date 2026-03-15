#if !defined(BALSA_OPTIMIZATION_GAUSSIAN_CONSTRAINT_ELIMINATION_HPP)
#define BALSA_OPTIMIZATION_GAUSSIAN_CONSTRAINT_ELIMINATION_HPP
#include <Eigen/Dense>
#include "balsa/eigen/concepts.hpp"
#include "balsa/iterator/enumerate.hpp"
#include "balsa/eigen/container_size.hpp"
namespace balsa::eigen {


    // Say we want to solve Ax = b given Cx = d
    // This function eliminates the constraints
    //
template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
auto gaussian_constraint_elimination(const ConstraintMat& C, const Mat &A) -> decltype(A.eval());











template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
void gaussian_constraint_elimination_inline(ConstraintMat& C, Mat &A)
{

    static_assert(ConstraintMat::Scalar == Mat::Scalar);
    using Scalar =typename ConstraintMat::Scalar;


    // for each row eliminate the constraint
    for(int constraint_row_index = 0; constraint_row_index < C.row_indexs(); ++constraint_row_index) {
        // find the pivot
        
        Eigen::SparseVector<Scalar> constraint_row = C.row(constraint_row_index);


        for(ConstraintMat::InnerIterator row_it(C); row_it; ++row_it) {
            
            
        }



        // eliminate other terms

    }

}

template<concepts::ColMajorSparseMatrix ConstraintMat, concepts::RowMajorSparseMatrix Mat>
auto gaussian_constraint_elimination(const ConstraintMat& C_, const Mat &A_)
{

    auto C = C_.eval();
    auto A = A_.eval();

    gaussian_constraint_elimination_inline();
    return A;

}
}// namespace balsa::eigen

