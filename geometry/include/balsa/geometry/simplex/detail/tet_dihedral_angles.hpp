#if !defined(BALSA_GEOMETRY_SIMPLEX_DETAIL_TET_DIHEDRAL_ANGLES_HPP)
#define BALSA_GEOMETRY_SIMPLEX_DETAIL_TET_DIHEDRAL_ANGLES_HPP

/// @file tet_dihedral_angles.hpp
/// @brief Shared helper: compute the 6 dihedral angles of a tetrahedron.
///
/// Two-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrapper)**: thin overload converting spans.
///
/// Used by both min_dihedral_angle and max_dihedral_angle to avoid code
/// duplication.

#include <array>
#include <balsa/geometry/simplex/concepts.hpp>
#include <cmath>
#include <numbers>
#include <span>

#include <zipper/Form.hpp>
#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex::detail {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Compute the outward-pointing unit normal of a triangular face.
///
/// @param e1, e2   Two edge vectors spanning the face.
/// @param opp      Vector from a face vertex to the opposite tet vertex.
/// @return         Unit normal pointing away from the opposite vertex.
///                 If the face is degenerate (zero-area), returns the zero
///                 vector.
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) == 3)
auto outward_face_normal(const V &e1, const V &e2, const V &opp)
    -> ::zipper::Vector<typename V::value_type, 3> {
    using T = typename V::value_type;
    ::zipper::Vector<T, 3> n = e1.cross(e2);
    if (n.dot(opp) > T{0}) { n *= T{-1}; }
    T len = n.norm();
    if (len > T{0}) { n /= len; }
    return n;
}

/// Compute the dihedral angle (in radians) between two faces given their
/// outward unit normals.
///
/// Dihedral angle = pi - arccos(n1 . n2).
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) == 3)
auto dihedral_from_normals(const V &n1, const V &n2) -> typename V::value_type {
    using T = typename V::value_type;
    T cos_normals = std::clamp(n1.dot(n2), T{-1}, T{1});
    return std::numbers::pi_v<T> - std::acos(cos_normals);
}

/// Compute all 6 dihedral angles of a tetrahedron, in radians (zipper core).
///
/// The returned array stores dihedral angles at edges:
///   [0] AB,  [1] AC,  [2] AD,  [3] BC,  [4] BD,  [5] CD.
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) == 3)
auto tet_dihedral_angles(const V &a, const V &b, const V &c, const V &d)
    -> std::array<typename V::value_type, 6> {
    using T = typename V::value_type;

    // Edge vectors from vertex A
    ::zipper::Vector<T, 3> ab = b - a;
    ::zipper::Vector<T, 3> ac = c - a;
    ::zipper::Vector<T, 3> ad = d - a;
    // Edge vectors from vertex B
    ::zipper::Vector<T, 3> bc = c - b;
    ::zipper::Vector<T, 3> bd = d - b;
    // Vector from B to A (for BCD face orientation)
    ::zipper::Vector<T, 3> ba = a - b;

    // Outward normals for each face:
    auto n_abc = outward_face_normal(ab, ac, ad);
    auto n_abd = outward_face_normal(ab, ad, ac);
    auto n_acd = outward_face_normal(ac, ad, ab);
    auto n_bcd = outward_face_normal(bc, bd, ba);

    return {{
        dihedral_from_normals(n_abc, n_abd), // AB
        dihedral_from_normals(n_abc, n_acd), // AC
        dihedral_from_normals(n_abd, n_acd), // AD
        dihedral_from_normals(n_abc, n_bcd), // BC
        dihedral_from_normals(n_abd, n_bcd), // BD
        dihedral_from_normals(n_acd, n_bcd), // CD
    }};
}

// ============================================================================
// Layer 1.5: PointSpan wrapper
// ============================================================================

/// Compute all 6 dihedral angles of a tetrahedron, in radians (PointSpan
/// wrapper).
template <PointSpan S>
    requires(dim_v<S> == 3)
auto tet_dihedral_angles(S a, S b, S c, S d) -> std::array<scalar_t<S>, 6> {
    return tet_dihedral_angles(::zipper::VectorBase(a),
                               ::zipper::VectorBase(b),
                               ::zipper::VectorBase(c),
                               ::zipper::VectorBase(d));
}

} // namespace balsa::geometry::simplex::detail
#endif
