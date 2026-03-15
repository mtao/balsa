#if !defined(BALSA_CONCEPTS_TENSOR_SHAPES_HPP)
#define BALSA_CONCEPTS_TENSOR_SHAPES_HPP
#include <array>
#include <zipper/concepts/Zipper.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/concepts/Form.hpp>
#include "balsa/tensor_types.hpp"

namespace balsa::concepts {

namespace detail {
    // extract the compile time rows of the input type
    template<::zipper::concepts::Zipper Type>
    constexpr index_type compile_row_size = Type::extents_type::static_extent(0);
    // extract the compile time cols of the input type
    template<::zipper::concepts::Zipper Type>
    constexpr index_type compile_col_size = Type::extents_type::static_extent(1);

    // output the compiletime shape in an array<int,2>
    template<::zipper::concepts::Zipper Mat>
    consteval std::array<int, 2> compile_shape_as_array() {
        return std::array<int, 2>{ { compile_row_size<Mat>,
                                     compile_col_size<Mat> } };
    }

    // returns if an eigen compile time shape is dynamic
    consteval bool has_dynamic_size(index_type eigen_shape_size) {
        return eigen_shape_size == std::dynamic_extent;
    }

    // returns if an eigen compile time shape is static
    consteval bool has_static_size(index_type eigen_shape_size) {
        return !has_dynamic_size(eigen_shape_size);
    }

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
concept RowStaticCompatible = ::zipper::concepts::Zipper<T> && detail::has_static_size(detail::compile_row_size<T>);

// Specifies if a matrix has static columns
template<typename T>
concept ColStaticCompatible = ::zipper::concepts::Zipper<T> && detail::has_static_size(detail::compile_col_size<T>);


// specifies if a matrix has static size
template<typename T>
concept RowColStaticCompatible = RowStaticCompatible<T> && ColStaticCompatible<T>;

// specifies if a matrix has a dynamic number of rows
template<typename T>
concept RowDynamicCompatible = ::zipper::concepts::Zipper<T> && detail::has_dynamic_size(detail::compile_row_size<T>);

// specifies if a matrix has a dynamic number of columns
template<typename T>
concept ColDynamicCompatible = ::zipper::concepts::Zipper<T> && detail::has_dynamic_size(detail::compile_col_size<T>);

template<index_type R, typename T>
concept RowCompatible = ::zipper::concepts::Zipper<T> && ((T::extents_type::static_extent(0) == R) || (T::extents_type::static_extent(0) == std::dynamic_extent));
template<index_type C, typename T>
concept ColCompatible = ::zipper::concepts::Zipper<T> && ((T::extents_type::static_extent(1) == C) || (T::extents_type::static_extent(1) == std::dynamic_extent));
template<index_type R, int C, typename T>
concept ShapeCompatible = ::zipper::concepts::Zipper<T> && RowCompatible<R, T> && ColCompatible<C, T>;


template<typename T>
concept VecCompatible = ::zipper::concepts::Vector<T>;
template<typename T>
concept RowVecCompatible = ::zipper::concepts::Form<T>;

template<typename T>
concept VecXCompatible = ::zipper::concepts::Vector<T> && RowDynamicCompatible<T>;
template<typename T>
concept RowVecXCompatible = ::zipper::concepts::Form<T> && RowDynamicCompatible<T>;


template<index_type D, typename T>
concept SquareMatrixDCompatible = ::zipper::concepts::Matrix<T> && ShapeCompatible<D, D, T>;

template<typename T>
concept SquareMatrix2Compatible = ::zipper::concepts::Matrix<T> && SquareMatrixDCompatible<2, T>;
template<typename T>
concept SquareMatrix3Compatible = ::zipper::concepts::Matrix<T> && SquareMatrixDCompatible<3, T>;
template<typename T>
concept SquareMatrix4Compatible = ::zipper::concepts::Matrix<T> && SquareMatrixDCompatible<4, T>;

template<typename T>
concept RowVecsDCompatible = ::zipper::concepts::Matrix<T> && ColStaticCompatible<T>;
template<typename T>
concept ColVecsDCompatible = ::zipper::concepts::Matrix<T> && RowStaticCompatible<T>;

template<typename T>
concept ColVecs2Compatible = ::zipper::concepts::Matrix<T> && RowCompatible<2, T>;
template<typename T>
concept ColVecs3Compatible = ::zipper::concepts::Matrix<T> && RowCompatible<3, T>;
template<typename T>
concept ColVecs4Compatible = ::zipper::concepts::Matrix<T> && RowCompatible<4, T>;


// TODO: will i someday figureo ut how to template this?
template<typename T>
concept RowVecs2Compatible = ::zipper::concepts::Matrix<T> && ColCompatible<2, T>;
template<typename T>
concept RowVecs3Compatible = ::zipper::concepts::Matrix<T> && ColCompatible<3, T>;
template<typename T>
concept RowVecs4Compatible = ::zipper::concepts::Matrix<T> && ColCompatible<4, T>;


template<index_type R, typename T>
concept VecDCompatible = ::zipper::concepts::Matrix<T> && ShapeCompatible<R, 1, T>;


template<typename T>
concept Vec2Compatible = VecDCompatible<2, T>;
template<typename T>
concept Vec3Compatible = VecDCompatible<3, T>;
template<typename T>
concept Vec4Compatible = VecDCompatible<4, T>;


namespace detail {

    template<concepts::RowColStaticCompatible MatType>
    consteval bool has_n_more_rows_than_cols(index_type n) {
        constexpr index_type crows = compile_row_size<MatType>;
        constexpr index_type ccols = compile_col_size<MatType>;
        return crows == ccols + n;
    }
}// namespace detail

}// namespace balsa::concepts
#endif
