#if !defined(BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP)
#define BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP
#include "matrix_types.hpp"

namespace balsa::eigen::concepts {

    namespace detail {
        template <EigenBaseDerived Type>
        constexpr int compile_row_size = Type::RowsAtCompileTime;
        template <EigenBaseDerived Type>
        constexpr int compile_col_size = Type::ColsAtCompileTime;

        constexpr bool is_dynamic(int eigen_shape_size)
        {
            return eigen_shape_size == Eigen::Dynamic;
        }

    }


    // Specifies if a matrix has static rows
template<typename T>
concept RowStaticCompatible = EigenBaseDerived<T> && !detail::is_dynamic(detail::compile_row_size<T>);

    // Specifies if a matrix has static columns
template<typename T>
concept ColStaticCompatible = EigenBaseDerived<T> && !detail::is_dynamic(detail::compile_col_size<T>);


// specifies if a matrix has static size
template<typename T>
concept RowColStaticCompatible = RowStaticCompatible<T> && ColStaticCompatible<T>;

// specifies if a matrix has a dynamic number of rows
template<typename T>
concept RowDynamicCompatible = EigenBaseDerived<T> && detail::is_dynamic(detail::compile_row_size<T>);

// specifies if a matrix has a dynamic number of columns
template<typename T>
concept ColDynamicCompatible = EigenBaseDerived<T> && detail::is_dynamic(detail::compile_col_size<T>);

template<int R, typename T>
concept RowCompatible = EigenBaseDerived<T> && ((T::RowsAtCompileTime == R) || (T::RowsAtCompileTime == Eigen::Dynamic));
template<int C, typename T>
concept ColCompatible = EigenBaseDerived<T> && ((T::ColsAtCompileTime == C) || (T::ColsAtCompileTime == Eigen::Dynamic));
template<int R, int C, typename T>
concept ShapeCompatible = EigenBaseDerived<T> &&RowCompatible<R, T> &&ColCompatible<C, T>;


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
