#if !defined(BALSA_ZIPPER_CONCEPTS_SHAPE_TYPES_HPP)
#define BALSA_ZIPPER_CONCEPTS_SHAPE_TYPES_HPP
#include <array>
#include "types.hpp"

namespace balsa::zipper::concepts {

namespace detail {
    // extract the compile time rows of the input type
    template<ZipperBaseDerived Type>
    constexpr int compile_row_size = Type::extents_type::static_extent(0);

    // extract the compile time cols of the input type
    template<ZipperBaseDerived Type>
    constexpr index_type compile_col_size = Type::static_extent::static_extent(1);

    // output the compiletime shape in an array<index_type,2>
    template<MatrixBaseDerived Mat>
    consteval std::array<index_type, 2> compile_shape_as_array() {
        return std::array<index_type, 2>{ { compile_row_size<Mat>,
                                            compile_col_size<Mat> } };
    }

    // returns if an zipper compile time shape is dynamic
    consteval bool has_dynamic_size(index_type zipper_shape_size) {
        return zipper_shape_size == std::dynamic_extent;
    }

    // returns if an zipper compile time shape is static
    consteval bool has_static_size(index_type zipper_shape_size) {
        return !has_dynamic_size(zipper_shape_size);
    }

    // TODO: convert to use zipper builtin tools
    // returns an offset of a compile size shape,
    consteval index_type relative_compile_size(index_type compile_size, index_type offset) {
        if (compile_size == std::dynamic_extent) {
            return std::dynamic_extent;
        } else {
            return compile_size + offset;
        }
    }

    consteval bool compatible_compile_size(index_type size_a, index_type size_b) {
        if (has_dynamic_size(size_a) || has_dynamic_size(size_b)) {
            return true;
        } else {
            return size_a == size_b;
        }
    }


}// namespace detail


// Specifies if a matrix has static rows
template<typename T>
concept RowStaticCompatible = ZipperBaseDerived<T> && detail::has_static_size(detail::compile_row_size<T>);

// Specifies if a matrix has static columns
template<typename T>
concept ColStaticCompatible = ZipperBaseDerived<T> && detail::has_static_size(detail::compile_col_size<T>);


// specifies if a matrix has static size
template<typename T>
concept RowColStaticCompatible = RowStaticCompatible<T> && ColStaticCompatible<T>;

// specifies if a matrix has a dynamic number of rows
template<typename T>
concept RowDynamicCompatible = ZipperBaseDerived<T> && detail::has_dynamic_size(detail::compile_row_size<T>);

// specifies if a matrix has a dynamic number of columns
template<typename T>
concept ColDynamicCompatible = ZipperBaseDerived<T> && detail::has_dynamic_size(detail::compile_col_size<T>);

template<index_type R, typename T>
concept RowCompatible = ZipperBaseDerived<T> && ((detail::compile_row_size<T> == R) || (detail::compile_row_size<T> == std::dynamic_extent));
template<index_type C, typename T>
concept ColCompatible = ZipperBaseDerived<T> && ((detail::compile_col_size<T> == C) || (detail::compile_col_size<T> == std::dynamic_extent));
template<index_type R, index_type C, typename T>
concept ShapeCompatible = ZipperBaseDerived<T> && RowCompatible<R, T> && ColCompatible<C, T>;


template<typename T>
concept VecCompatible = MatrixBaseDerived<T> && ColCompatible<1, T>;
template<typename T>
concept RowVecCompatible = MatrixBaseDerived<T> && RowCompatible<1, T>;

template<typename T>
concept VecXCompatible = MatrixBaseDerived<T> && RowDynamicCompatible<T>;
template<typename T>
concept RowVecXCompatible = MatrixBaseDerived<T> && ColDynamicCompatible<T>;


template<index_type D, typename T>
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

    template<zipper::concepts::RowColStaticCompatible MatType>
    consteval bool has_n_more_rows_than_cols(int n) {
        constexpr int crows = compile_row_size<MatType>;
        constexpr int ccols = compile_col_size<MatType>;
        return crows == ccols + n;
    }
}// namespace detail

}// namespace balsa::zipper::concepts
#endif
