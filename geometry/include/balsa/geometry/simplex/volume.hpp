#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP

// Simplex volume computation.
//
// Delegates to quiver::simplex for the canonical implementations.
// Eigen overloads convert to zipper and forward.

#include <quiver/simplex/volume.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
auto volume_signed(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_signed(V);
}

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_unsigned(V);
}

/// Convenience wrapper: signed when D+1 == cols (full-rank simplex),
/// unsigned otherwise.
template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type {
    if (V.extent(0) + 1 == V.extent(1)) {
        return ::quiver::simplex::volume_signed(V);
    } else {
        return ::quiver::simplex::volume_unsigned(V);
    }
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
    auto Z = eigen::as_zipper(V);
    if (Z.extent(0) + 1 == Z.extent(1)) {
        return ::quiver::simplex::volume_signed(Z);
    } else {
        return ::quiver::simplex::volume_unsigned(Z);
    }
}

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
