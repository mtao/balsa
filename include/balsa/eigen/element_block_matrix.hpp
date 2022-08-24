#if !defined(BALSA_EIGEN_SHAPE_CHECKS_HPP)
#define BALSA_EIGEN_SHAPE_CHECKS_HPP
#include <Eigen/Core>
#include <Eigen/Sparse>
#include "concepts/matrix_types.hpp"
#include "../data_structures/stacked_contiguous_buffer.hpp"

namespace balsa::eigen {

template<concepts::EigenBaseDerived BlockMatrixType>
class ElementBlockMatrix;
}

namespace Eigen {
namespace internal {
    // ElementBlockMatrix looks-like a SparseMatrix, so let's inherits its traits:
    template<balsa::eigen::concepts::EigenBaseDerived BlockMatrixType>
    struct traits<balsa::eigen::ElementBlockMatrix<BlockMatrixType>> : public Eigen::internal::traits<Eigen::SparseMatrix<double>> {};
}
}

namespace balsa::eigen {


//    namespace detail {
//    template <concepts::EigenBaseDerived MatrixType_>
// class MatrixBlock : public Eigen::EigenBase<MatrixBlock<MatrixType_> > {
// public:
//    using MatrixType = MatrixType_;
//  // Required typedefs, constants, and method:
//    using Scalar = typename MatrixType::Scalar;
//    using RealScalar = Scalar;
//    using StorageIndex = typename MatrixType::StorageIndex;
//    using Index = Eigen::Index;
//  enum {
//    ColsAtCompileTime = Eigen::Dynamic,
//    MaxColsAtCompileTime = Eigen::Dynamic,
//    IsRowMajor = Eigen::internal::template traits<MatrixType>::IsRowMajor
//  };
//
//
//  template<typename Rhs>
//  Eigen::Product<MatrixBlock,Rhs,Eigen::AliasFreeProduct> operator*(const Eigen::MatrixBase<Rhs>& x) const {
//    return Eigen::Product<MatrixBlock,Rhs,Eigen::AliasFreeProduct>(*this, x.derived());
//  }
//
//
// private:
//
//  MatrixType
//  std::vector<Index> _rows;
//  std::vector<Index> _cols;
//
//
//};
//    }

// Example of a matrix-free wrapper from a user type to Eigen's compatible type
// For the sake of simplicity, this example simply wrap a Eigen::SparseMatrix.
template<concepts::EigenBaseDerived BlockMatrixType_>
class ElementBlockMatrix : public Eigen::EigenBase<ElementBlockMatrix<BlockMatrixType_>> {
  public:
    using BlockMatrixType = BlockMatrixType_;
    // Required typedefs, constants, and method:
    using Scalar = typename BlockMatrixType::Scalar;
    using RealScalar = Scalar;
    using StorageIndex = typename BlockMatrixType::StorageIndex;
    using Index = Eigen::Index;
    enum {
        ColsAtCompileTime = Eigen::Dynamic,
        MaxColsAtCompileTime = Eigen::Dynamic,
        IsRowMajor = Eigen::internal::template traits<BlockMatrixType>::IsRowMajor
    };

    Index rows() const { return _num_rows; }
    Index cols() const { return _num_cols; }

    template<typename Rhs>
    Eigen::Product<ElementBlockMatrix, Rhs, Eigen::AliasFreeProduct> operator*(const Eigen::MatrixBase<Rhs> &x) const {
        return Eigen::Product<ElementBlockMatrix, Rhs, Eigen::AliasFreeProduct>(*this, x.derived());
    }

    const BlockMatrixType &get_block(size_t index) const {
        return _blocks.at(index);
    }
    const 
        const auto &row_block = _row_blocks.get_span(index);
        const auto &col_block = _col_blocks.get_span(index);
    }

    // Custom API:
  private:
    data_structures::StackedContiguousBuffer<Index>
      _row_blocks;
    data_structures::StackedContiguousBuffer<Index> _col_blocks;
    std::vector<BlockMatrixType> _blocks;

    Index _num_rows;
    Index _num_cols;
};


}// namespace balsa::eigen
// Implementation of ElementBlockMatrix * Eigen::DenseVector though a specialization of internal::generic_product_impl:
namespace Eigen {
namespace internal {

    template<typename Rhs>
    struct generic_product_impl<vem::ElementBlockMatrix, Rhs, SparseShape, DenseShape, GemvProduct>// GEMV stands for matrix-vector
      : generic_product_impl_base<vem::ElementBlockMatrix, Rhs, generic_product_impl<balsa::eigen::ElementBlockMatrix, Rhs>> {
        typedef typename Product<vem::ElementBlockMatrix, Rhs>::Scalar Scalar;

        template<typename Dest>
        static void scaleAndAddTo(Dest &dst, const ElementBlockMatrix &lhs, const Rhs &rhs, const Scalar &alpha) {
            // This method should implement "dst += alpha * lhs * rhs" inplace,
            // however, for iterative solvers, alpha is always equal to 1, so let's not bother about it.
            assert(alpha == Scalar(1) && "scaling is not implemented");
            EIGEN_ONLY_USED_FOR_DEBUG(alpha);

            // Here we could simply call dst.noalias() += lhs.my_matrix() * rhs,
            // but let's do something fancier (and less efficient):
            for (Index i = 0; i < lhs.cols(); ++i)
                dst += rhs(i) * lhs.my_matrix().col(i);
        }
    };

}
#endif
