#if !defined(BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP)
#define BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP
#include "matrix_types.hpp"

namespace balsa::eigen::concepts {

template<typename T>
concept RowStaticCompatible = DenseBaseDerived<T> && (T::RowsAtCompileTime != Eigen::Dynamic);

template<typename T>
concept ColStaticCompatible = DenseBaseDerived<T> && (T::ColsAtCompileTime != Eigen::Dynamic);

template<typename T>
concept RowDynamicCompatible = DenseBaseDerived<T> && (T::RowsAtCompileTime == Eigen::Dynamic);

template<typename T>
concept ColDynamicCompatible = DenseBaseDerived<T> && (T::ColsAtCompileTime == Eigen::Dynamic);

template<int R, typename T>
concept RowCompatible = DenseBaseDerived<T> && ((T::RowsAtCompileTime == R) || (T::RowsAtCompileTime == Eigen::Dynamic));
template<int C, typename T>
concept ColCompatible = DenseBaseDerived<T> && ((T::ColsAtCompileTime == C) || (T::ColsAtCompileTime == Eigen::Dynamic));
template<int R, int C, typename T>
concept ShapeCompatible = DenseBaseDerived<T> &&RowCompatible<R, T> &&ColCompatible<C, T>;


template<typename T>
concept VecCompatible = MatrixBaseDerived<T> &&ColCompatible<1, T>;
template<typename T>
concept RowVecCompatible = MatrixBaseDerived<T> &&RowCompatible<1, T>;

template<typename T>
concept VecXCompatible = MatrixBaseDerived<T> &&RowDynamicCompatible<T>;
template<typename T>
concept RowVecXCompatible = MatrixBaseDerived<T> &&ColDynamicCompatible<T>;


template<int D, typename T>
concept SquareMatrixDCompatible = MatrixBaseDerived<T> &&ShapeCompatible<D, D, T>;

template<typename T>
concept SquareMatrix2Compatible = MatrixBaseDerived<T> &&SquareMatrixDCompatible<2, T>;
template<typename T>
concept SquareMatrix3Compatible = MatrixBaseDerived<T> &&SquareMatrixDCompatible<3, T>;
template<typename T>
concept SquareMatrix4Compatible = MatrixBaseDerived<T> &&SquareMatrixDCompatible<4, T>;

template<typename T>
concept RowVecsDCompatible = MatrixBaseDerived<T> &&ColStaticCompatible<T>;
template<typename T>
concept ColVecsDCompatible = MatrixBaseDerived<T> &&RowStaticCompatible<T>;

template<typename T>
concept ColVecs2Compatible = MatrixBaseDerived<T> &&RowCompatible<2, T>;
template<typename T>
concept ColVecs3Compatible = MatrixBaseDerived<T> &&RowCompatible<3, T>;
template<typename T>
concept ColVecs4Compatible = MatrixBaseDerived<T> &&RowCompatible<4, T>;


// TODO: will i someday figureo ut how to template this?
template<typename T>
concept RowVecs2Compatible = MatrixBaseDerived<T> &&ColCompatible<2, T>;
template<typename T>
concept RowVecs3Compatible = MatrixBaseDerived<T> &&ColCompatible<3, T>;
template<typename T>
concept RowVecs4Compatible = MatrixBaseDerived<T> &&ColCompatible<4, T>;


template<int R, typename T>
concept VecDCompatible = MatrixBaseDerived<T> &&ShapeCompatible<R, 1, T>;


template<typename T>
concept Vec2Compatible = VecDCompatible<2, T>;
template<typename T>
concept Vec3Compatible = VecDCompatible<3, T>;
template<typename T>
concept Vec4Compatible = VecDCompatible<4, T>;


}// namespace balsa::eigen::concepts
#endif
