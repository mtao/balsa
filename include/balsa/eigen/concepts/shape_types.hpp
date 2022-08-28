#if !defined(BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP)
#define BALSA_EIGEN_CONCEPTS_SHAPE_TYPES_HPP
#include "matrix_types.hpp"
#include <array>

namespace balsa::eigen::concepts {

namespace detail {
    // extract the compile time rows of the input type
    template<EigenBaseDerived Type>
    constexpr int compile_row_size = Type::RowsAtCompileTime;
    // extract the compile time cols of the input type
    template<EigenBaseDerived Type>
    constexpr int compile_col_size = Type::ColsAtCompileTime;

    // output the compiletime shape in an array<int,2>
    template<eigen::concepts::EigenBaseDerived Mat>
    consteval std::array<int, 2> compile_shape_as_array() {
        return std::array<int, 2>{ { compile_row_size<Mat>,
                                     compile_col_size<Mat> } };
    }

    // returns if an eigen compile time shape is dynamic
    consteval bool has_dynamic_size(int eigen_shape_size) {
        return eigen_shape_size == Eigen::Dynamic;
    }

    // returns if an eigen compile time shape is static
    consteval bool has_static_size(int eigen_shape_size) {
        return !has_dynamic_size(eigen_shape_size);
    }

    // returns an offset of a compile size shape,
    consteval int relative_compile_size(int compile_size, int offset) {
        if (compile_size == Eigen::Dynamic) {
            return Eigen::Dynamic;
        } else {
            return compile_size - offset;
        }
    }

    consteval bool compatible_compile_size(int size_a, int size_b) {
        if (has_dynamic_size(size_a) || has_dynamic_size(size_b)) {
            return true;
        } else {
            return size_a == size_b;
        }
    }




}// namespace detail


// Specifies if a matrix has static rows
template<typename T>
concept RowStaticCompatible = EigenBaseDerived<T> && detail::has_static_size(detail::compile_row_size<T>);

// Specifies if a matrix has static columns
template<typename T>
concept ColStaticCompatible = EigenBaseDerived<T> && detail::has_static_size(detail::compile_col_size<T>);


// specifies if a matrix has static size
template<typename T>
concept RowColStaticCompatible = RowStaticCompatible<T> && ColStaticCompatible<T>;

// specifies if a matrix has a dynamic number of rows
template<typename T>
concept RowDynamicCompatible = EigenBaseDerived<T> && detail::has_dynamic_size(detail::compile_row_size<T>);

// specifies if a matrix has a dynamic number of columns
template<typename T>
concept ColDynamicCompatible = EigenBaseDerived<T> && detail::has_dynamic_size(detail::compile_col_size<T>);

template<int R, typename T>
concept RowCompatible = EigenBaseDerived<T> &&((T::RowsAtCompileTime == R) || (T::RowsAtCompileTime == Eigen::Dynamic));
template<int C, typename T>
concept ColCompatible = EigenBaseDerived<T> &&((T::ColsAtCompileTime == C) || (T::ColsAtCompileTime == Eigen::Dynamic));
template<int R, int C, typename T>
concept ShapeCompatible = EigenBaseDerived<T> && RowCompatible<R, T> && ColCompatible<C, T>;


template<typename T>
concept VecCompatible = MatrixBaseDerived<T> && ColCompatible<1, T>;
template<typename T>
concept RowVecCompatible = MatrixBaseDerived<T> && RowCompatible<1, T>;

template<typename T>
concept VecXCompatible = MatrixBaseDerived<T> && RowDynamicCompatible<T>;
template<typename T>
concept RowVecXCompatible = MatrixBaseDerived<T> && ColDynamicCompatible<T>;


template<int D, typename T>
concept SquareMatrixDCompatible = MatrixBaseDerived<T> && ShapeCompatible<D, D, T>;

template<typename T>
concept SquareMatrix2Compatible = MatrixBaseDerived<T> && SquareMatrixDCompatible<2, T>;
template<typename T>
concept SquareMatrix3Compatible = MatrixBaseDerived<T> && SquareMatrixDCompatible<3, T>;
template<typename T>
concept SquareMatrix4Compatible = MatrixBaseDerived<T> && SquareMatrixDCompatible<4, T>;

template<typename T>
concept RowVecsDCompatible = MatrixBaseDerived<T> && ColStaticCompatible<T>;
template<typename T>
concept ColVecsDCompatible = MatrixBaseDerived<T> && RowStaticCompatible<T>;

template<typename T>
concept ColVecs2Compatible = MatrixBaseDerived<T> && RowCompatible<2, T>;
template<typename T>
concept ColVecs3Compatible = MatrixBaseDerived<T> && RowCompatible<3, T>;
template<typename T>
concept ColVecs4Compatible = MatrixBaseDerived<T> && RowCompatible<4, T>;


// TODO: will i someday figureo ut how to template this?
template<typename T>
concept RowVecs2Compatible = MatrixBaseDerived<T> && ColCompatible<2, T>;
template<typename T>
concept RowVecs3Compatible = MatrixBaseDerived<T> && ColCompatible<3, T>;
template<typename T>
concept RowVecs4Compatible = MatrixBaseDerived<T> && ColCompatible<4, T>;


template<int R, typename T>
concept VecDCompatible = MatrixBaseDerived<T> && ShapeCompatible<R, 1, T>;


template<typename T>
concept Vec2Compatible = VecDCompatible<2, T>;
template<typename T>
concept Vec3Compatible = VecDCompatible<3, T>;
template<typename T>
concept Vec4Compatible = VecDCompatible<4, T>;


namespace detail {

    template <eigen::concepts::RowColStaticCompatible MatType>
    consteval bool has_n_more_rows_than_cols(int n)
    {
        constexpr int crows = compile_row_size<MatType>;
        constexpr int ccols = compile_row_size<MatType>;
        return crows + n == ccols;
    }
}

}// namespace balsa::eigen::concepts
#endif
