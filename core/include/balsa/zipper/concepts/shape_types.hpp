#if !defined(BALSA_ZIPPER_CONCEPTS_SHAPE_TYPES_HPP)
#define BALSA_ZIPPER_CONCEPTS_SHAPE_TYPES_HPP
// This header re-exports the canonical shape concepts from balsa::concepts
// into the balsa::zipper::concepts namespace for backward compatibility.
// The single source of truth is balsa/concepts/tensor_shapes.hpp.
#include "balsa/concepts/tensor_shapes.hpp"

namespace balsa::zipper::concepts {

namespace detail {
    using balsa::concepts::detail::compile_row_size;
    using balsa::concepts::detail::compile_col_size;
    using balsa::concepts::detail::compile_shape_as_array;
    using balsa::concepts::detail::has_dynamic_size;
    using balsa::concepts::detail::has_static_size;
    using balsa::concepts::detail::relative_compile_size;
    using balsa::concepts::detail::compatible_compile_size;
    using balsa::concepts::detail::has_n_more_rows_than_cols;
}// namespace detail

using balsa::concepts::RowStaticCompatible;
using balsa::concepts::ColStaticCompatible;
using balsa::concepts::RowColStaticCompatible;
using balsa::concepts::RowDynamicCompatible;
using balsa::concepts::ColDynamicCompatible;
using balsa::concepts::RowCompatible;
using balsa::concepts::ColCompatible;
using balsa::concepts::ShapeCompatible;
using balsa::concepts::VecCompatible;
using balsa::concepts::RowVecCompatible;
using balsa::concepts::VecXCompatible;
using balsa::concepts::RowVecXCompatible;
using balsa::concepts::SquareMatrixDCompatible;
using balsa::concepts::SquareMatrix2Compatible;
using balsa::concepts::SquareMatrix3Compatible;
using balsa::concepts::SquareMatrix4Compatible;
using balsa::concepts::RowVecsDCompatible;
using balsa::concepts::ColVecsDCompatible;
using balsa::concepts::ColVecs2Compatible;
using balsa::concepts::ColVecs3Compatible;
using balsa::concepts::ColVecs4Compatible;
using balsa::concepts::RowVecs2Compatible;
using balsa::concepts::RowVecs3Compatible;
using balsa::concepts::RowVecs4Compatible;
using balsa::concepts::VecDCompatible;
using balsa::concepts::Vec2Compatible;
using balsa::concepts::Vec3Compatible;
using balsa::concepts::Vec4Compatible;

}// namespace balsa::zipper::concepts
#endif
