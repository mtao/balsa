#if !defined(BALSA_EIGEN_CONCEPTS_MATRIX_TYPES_HPP)
#define BALSA_EIGEN_CONCEPTS_MATRIX_TYPES_HPP


#include <concepts>
#include <Eigen/Core>
#include <Eigen/Sparse>

namespace balsa::eigen::concepts {

namespace detail {
    template<typename T>
    concept MatrixBaseDerived = std::derived_from<T, typename Eigen::MatrixBase<T>>;

    template<typename T>
    concept ArrayBaseDerived = std::derived_from<T, typename Eigen::ArrayBase<T>>;

    template<typename T>
    concept DenseBaseDerived = std::derived_from<T, typename Eigen::DenseBase<T>>;


    template<typename T>
    concept PlainObjectBaseDerived = std::derived_from<T, typename Eigen::PlainObjectBase<T>>;

    template<typename T>
    concept EigenBaseDerived = std::derived_from<T, typename Eigen::EigenBase<T>>;

    template<typename T>
    concept SparseCompressedBaseDerived = std::derived_from<T, typename Eigen::SparseCompressedBase<T>>;

    template<typename T>
    concept SparseMatrixBaseDerived = std::derived_from<T, typename Eigen::SparseMatrixBase<T>>;


    template<typename T>
    using derived_type = std::decay_t<decltype(std::declval<T>().derived())>;
}// namespace detail

template<typename T>
concept MatrixBaseDerived = detail::MatrixBaseDerived<detail::derived_type<T>>;

template<typename T>
concept ArrayBaseDerived = detail::ArrayBaseDerived<detail::derived_type<T>>;

template<typename T>
concept DenseBaseDerived = detail::DenseBaseDerived<detail::derived_type<T>>;


template<typename T>
concept PlainObjectBaseDerived = detail::PlainObjectBaseDerived<detail::derived_type<T>>;

template<typename T>
concept EigenBaseDerived = detail::EigenBaseDerived<detail::derived_type<T>>;

template<typename T>
concept SparseCompressedBaseDerived = std::derived_from<T, typename Eigen::SparseCompressedBase<T>>;

template<typename T>
concept SparseMatrixBaseDerived = std::derived_from<T, typename Eigen::SparseMatrixBase<T>>;

template<typename T>
concept IsEigenMatrix = MatrixBaseDerived<T> || SparseMatrixBaseDerived<T>;

template<typename T>
concept IntegralMatrix = MatrixBaseDerived<T> &&std::is_integral_v<typename T::Scalar>;


template<typename T>
concept RowMajorSparseMatrix = SparseMatrixBaseDerived<T> && T::IsRowMajor;

template<typename T>
concept ColMajorSparseMatrix = SparseMatrixBaseDerived<T> && !T::IsRowMajor;
}// namespace balsa::eigen::concepts

#endif
