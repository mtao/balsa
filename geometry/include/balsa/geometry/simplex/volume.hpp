#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP

// Simplex volume computation.
//
// The zipper-concept implementations live in quiver::simplex and are
// re-exported here for backward compatibility.  The Eigen overloads
// convert to zipper and delegate.

#include <quiver/simplex/volume.hpp>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto volume_signed(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_signed(V);
}

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_unsigned(V);
}

template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume(V);
}

// ── Eigen overloads (convert to zipper, then delegate) ─────────────────

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar {
    return ::quiver::simplex::volume_signed(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar {
    return ::quiver::simplex::volume_unsigned(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar {
    return ::quiver::simplex::volume(eigen::as_zipper(V));
}

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
