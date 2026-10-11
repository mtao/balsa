#if !defined(BALSA_GEOMETRY_SPATIAL_CONCEPTS_HPP)
#define BALSA_GEOMETRY_SPATIAL_CONCEPTS_HPP

/// @file concepts.hpp
/// @brief Re-exports algebraic concepts from zipper for spatial use.
///
/// Scalar concepts (ring, field) constrain KDOP/BVH scalar type
/// parameters.  Structure concepts (module, vector_space, etc.) are
/// available for downstream code that operates on spatial vectors.
///
/// See <zipper/concepts/Algebraic.hpp>, <zipper/concepts/Module.hpp>,
/// and <zipper/concepts/InnerProductSpace.hpp> for full documentation.

#include <zipper/concepts/Algebraic.hpp>
#include <zipper/concepts/InnerProductSpace.hpp>
#include <zipper/concepts/Module.hpp>

namespace balsa::geometry::spatial {

// Scalar concepts
using ::zipper::concepts::field;
using ::zipper::concepts::ring;

// Structure concepts
using ::zipper::concepts::inner_product_space;
using ::zipper::concepts::module;
using ::zipper::concepts::module_over;
using ::zipper::concepts::normed_space;
using ::zipper::concepts::vector_space;
using ::zipper::concepts::vector_space_over;

} // namespace balsa::geometry::spatial
#endif
