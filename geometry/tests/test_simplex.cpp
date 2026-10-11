/// @file test_simplex.cpp
/// @brief Tests for the balsa::geometry::simplex module (Layer 1, Layer 1.5,
/// Layer 1.5M).
///
/// Tests all simplex pure functions: volume, circumcenter,
/// point_in_circumsphere, and quality metrics.
/// Uses only zipper + Catch2 (no quiver mesh/attribute dependency).

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// All simplex headers
#include <balsa/geometry/simplex/amips.hpp>
#include <balsa/geometry/simplex/aspect_ratio.hpp>
#include <balsa/geometry/simplex/circumcenter.hpp>
#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/detail/tet_dihedral_angles.hpp>
#include <balsa/geometry/simplex/max_angle.hpp>
#include <balsa/geometry/simplex/min_angle.hpp>
#include <balsa/geometry/simplex/point_in_circumcircle.hpp>
#include <balsa/geometry/simplex/radius_ratio.hpp>
#include <balsa/geometry/simplex/scaled_jacobian.hpp>
#include <balsa/geometry/simplex/volume.hpp>
// New primitive headers
#include <balsa/geometry/simplex/boundary_volume.hpp>
#include <balsa/geometry/simplex/circumradius.hpp>
#include <balsa/geometry/simplex/closest_point.hpp>
#include <balsa/geometry/simplex/edge_length.hpp>
#include <balsa/geometry/simplex/edge_matrix.hpp>
#include <balsa/geometry/simplex/face_normal.hpp>
#include <balsa/geometry/simplex/inradius.hpp>
#include <balsa/geometry/simplex/jacobian.hpp>
#include <balsa/geometry/simplex/ray_intersection.hpp>

#include <zipper/Matrix.hpp>
#include <zipper/utils/determinant.hpp>

using namespace balsa::geometry::simplex;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// ============================================================================
// Helper: make a fixed-size const span from an array
// ============================================================================
template <typename T, std::size_t N>
auto sp(const std::array<T, N> &arr) -> std::span<const T, N> {
    return std::span<const T, N>(arr);
}

// ============================================================================
// Test geometry constants
// ============================================================================

// Equilateral triangle in 2D (side length 1)
static constexpr std::array<double, 2> eq_a{0.0, 0.0};
static constexpr std::array<double, 2> eq_b{1.0, 0.0};
static constexpr std::array<double, 2> eq_c{0.5, std::numbers::sqrt3 / 2.0};

// Right triangle in 2D
static constexpr std::array<double, 2> rt_a{0.0, 0.0};
static constexpr std::array<double, 2> rt_b{1.0, 0.0};
static constexpr std::array<double, 2> rt_c{0.0, 1.0};

// Degenerate (collinear) triangle
static constexpr std::array<double, 2> dg_a{0.0, 0.0};
static constexpr std::array<double, 2> dg_b{1.0, 0.0};
static constexpr std::array<double, 2> dg_c{2.0, 0.0};

// Regular tetrahedron in 3D (edge length 1)
static constexpr double sqrt3 = std::numbers::sqrt3;
static constexpr double sqrt6 =
    2.44948974278317809819728407470589139196594748328;
static constexpr std::array<double, 3> tet_a{0.0, 0.0, 0.0};
static constexpr std::array<double, 3> tet_b{1.0, 0.0, 0.0};
static constexpr std::array<double, 3> tet_c{0.5, sqrt3 / 2.0, 0.0};
static constexpr std::array<double, 3> tet_d{0.5, sqrt3 / 6.0, sqrt6 / 3.0};

// Degenerate tet (coplanar)
static constexpr std::array<double, 3> dtet_a{0.0, 0.0, 0.0};
static constexpr std::array<double, 3> dtet_b{1.0, 0.0, 0.0};
static constexpr std::array<double, 3> dtet_c{0.0, 1.0, 0.0};
static constexpr std::array<double, 3> dtet_d{1.0, 1.0, 0.0};

// Right-angle tet: one corner at origin with three mutually perpendicular edges
static constexpr std::array<double, 3> rtet_a{0.0, 0.0, 0.0};
static constexpr std::array<double, 3> rtet_b{1.0, 0.0, 0.0};
static constexpr std::array<double, 3> rtet_c{0.0, 1.0, 0.0};
static constexpr std::array<double, 3> rtet_d{0.0, 0.0, 1.0};

// ============================================================================
// Concepts
// ============================================================================
TEST_CASE("simplex_scalar_concept", "[simplex][concepts]") {
    static_assert(SimplexScalar<float>);
    static_assert(SimplexScalar<double>);
    static_assert(SimplexScalar<long double>);
    static_assert(!SimplexScalar<int>);
    static_assert(!SimplexScalar<unsigned>);
}

TEST_CASE("point_span_concept", "[simplex][concepts]") {
    static_assert(PointSpan<std::span<const double, 2>>);
    static_assert(PointSpan<std::span<const double, 3>>);
    static_assert(PointSpan<std::span<const float, 3>>);
    // Dynamic extent should NOT satisfy PointSpan
    static_assert(!PointSpan<std::span<const double>>);
}

// ============================================================================
// Volume — Signed (triangle 2D)
// ============================================================================
TEST_CASE("volume_signed_equilateral_2d", "[simplex][volume][triangle]") {
    auto result = volume_signed(sp(eq_a), sp(eq_b), sp(eq_c));
    // Equilateral with side 1: area = sqrt(3)/4
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

TEST_CASE("volume_signed_right_2d", "[simplex][volume][triangle]") {
    auto result = volume_signed(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(result, WithinAbs(0.5, 1e-14));
}

TEST_CASE("volume_signed_degenerate_2d", "[simplex][volume][triangle]") {
    auto result = volume_signed(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(result, WithinAbs(0.0, 1e-14));
}

TEST_CASE("volume_signed_orientation_2d", "[simplex][volume][triangle]") {
    // Reversing vertex order should flip sign
    auto pos = volume_signed(sp(eq_a), sp(eq_b), sp(eq_c));
    auto neg = volume_signed(sp(eq_a), sp(eq_c), sp(eq_b));
    CHECK_THAT(pos, WithinAbs(-neg, 1e-14));
}

// ============================================================================
// Volume — Unsigned (triangle)
// ============================================================================
TEST_CASE("volume_unsigned_triangle_2d", "[simplex][volume][triangle]") {
    auto result = volume_unsigned(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

TEST_CASE("volume_unsigned_triangle_3d_embedded",
          "[simplex][volume][triangle][3d]") {
    // Same triangle embedded in 3D (z=0)
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto result = volume_unsigned(sp(a), sp(b), sp(c));
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

// ============================================================================
// Volume — Signed (tet)
// ============================================================================
TEST_CASE("volume_signed_tet_regular", "[simplex][volume][tet]") {
    auto result = volume_signed(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Regular tet with edge length 1: V = 1 / (6*sqrt(2))
    double expected = 1.0 / (6.0 * std::numbers::sqrt2);
    CHECK_THAT(result, WithinAbs(expected, 1e-12));
}

TEST_CASE("volume_signed_tet_right", "[simplex][volume][tet]") {
    auto result = volume_signed(sp(rtet_a), sp(rtet_b), sp(rtet_c), sp(rtet_d));
    // Right-angle tet with unit edges: V = 1/6
    CHECK_THAT(result, WithinAbs(1.0 / 6.0, 1e-14));
}

TEST_CASE("volume_signed_tet_degenerate", "[simplex][volume][tet]") {
    auto result = volume_signed(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK_THAT(result, WithinAbs(0.0, 1e-14));
}

// ============================================================================
// Volume — Generic signed/unsigned
// ============================================================================
TEST_CASE("volume_signed_triangle_2d", "[simplex][volume][generic]") {
    auto result = volume_signed(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

TEST_CASE("volume_unsigned_triangle_3d", "[simplex][volume][generic]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto result = volume_unsigned(sp(a), sp(b), sp(c));
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

TEST_CASE("volume_signed_edge", "[simplex][volume][generic]") {
    // 1-simplex in 1D: just the distance
    std::array<double, 1> a{0.0};
    std::array<double, 1> b{3.0};
    auto result = volume_signed(sp(a), sp(b));
    CHECK_THAT(result, WithinAbs(3.0, 1e-14));
}

// ============================================================================
// Circumcenter — Triangle 2D
// ============================================================================
TEST_CASE("circumcenter_equilateral_2d", "[simplex][circumcenter][triangle]") {
    auto cc = circumcenter(sp(eq_a), sp(eq_b), sp(eq_c));
    // Circumcenter of equilateral triangle = centroid
    CHECK_THAT(cc[0], WithinAbs(0.5, 1e-12));
    CHECK_THAT(cc[1], WithinAbs(sqrt3 / 6.0, 1e-12));
}

TEST_CASE("circumcenter_right_2d", "[simplex][circumcenter][triangle]") {
    auto cc = circumcenter(sp(rt_a), sp(rt_b), sp(rt_c));
    // Circumcenter of right triangle is at midpoint of hypotenuse
    CHECK_THAT(cc[0], WithinAbs(0.5, 1e-12));
    CHECK_THAT(cc[1], WithinAbs(0.5, 1e-12));
}

TEST_CASE("circumcenter_edge_midpoint", "[simplex][circumcenter]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{4.0, 2.0};
    auto cc = circumcenter(sp(a), sp(b));
    CHECK_THAT(cc[0], WithinAbs(2.0, 1e-14));
    CHECK_THAT(cc[1], WithinAbs(1.0, 1e-14));
}

TEST_CASE("circumcenter_with_radius_equilateral",
          "[simplex][circumcenter][triangle]") {
    auto [cc, r] = circumcenter_with_radius(sp(eq_a), sp(eq_b), sp(eq_c));
    // Circumradius of equilateral triangle with side 1: R = 1/sqrt(3)
    CHECK_THAT(r, WithinAbs(1.0 / sqrt3, 1e-12));
}

// ============================================================================
// Circumcenter — Tet
// ============================================================================
TEST_CASE("circumcenter_regular_tet", "[simplex][circumcenter][tet]") {
    auto cc = circumcenter(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Circumcenter of regular tet should be equidistant from all vertices.
    // Check distance from cc to each vertex is the same.
    auto dist_sq = [&](const std::array<double, 3> &v) {
        return (cc[0] - v[0]) * (cc[0] - v[0]) + (cc[1] - v[1]) * (cc[1] - v[1])
               + (cc[2] - v[2]) * (cc[2] - v[2]);
    };
    double d0 = dist_sq(tet_a);
    double d1 = dist_sq(tet_b);
    double d2 = dist_sq(tet_c);
    double d3 = dist_sq(tet_d);
    CHECK_THAT(d0, WithinAbs(d1, 1e-10));
    CHECK_THAT(d0, WithinAbs(d2, 1e-10));
    CHECK_THAT(d0, WithinAbs(d3, 1e-10));
}

// ============================================================================
// Point in Circumsphere
// ============================================================================
TEST_CASE("point_in_circumsphere_inside", "[simplex][circumsphere]") {
    // Point at centroid of equilateral triangle is inside circumcircle
    std::array<double, 2> centroid{0.5, sqrt3 / 6.0};
    CHECK(point_in_circumsphere(sp(centroid), sp(eq_a), sp(eq_b), sp(eq_c)));
}

TEST_CASE("point_in_circumsphere_outside", "[simplex][circumsphere]") {
    // Point far away
    std::array<double, 2> far_pt{10.0, 10.0};
    CHECK_FALSE(
        point_in_circumsphere(sp(far_pt), sp(eq_a), sp(eq_b), sp(eq_c)));
}

TEST_CASE("point_in_circumsphere_tet_inside", "[simplex][circumsphere][tet]") {
    // Centroid of regular tet should be inside circumsphere
    std::array<double, 3> centroid{
        (tet_a[0] + tet_b[0] + tet_c[0] + tet_d[0]) / 4.0,
        (tet_a[1] + tet_b[1] + tet_c[1] + tet_d[1]) / 4.0,
        (tet_a[2] + tet_b[2] + tet_c[2] + tet_d[2]) / 4.0};
    CHECK(point_in_circumsphere(
        sp(centroid), sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)));
}

// ============================================================================
// Radius Ratio — Triangle
// ============================================================================
TEST_CASE("simplex_radius_ratio_triangle_equilateral",
          "[simplex][radius_ratio][triangle]") {
    auto result = radius_ratio(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("simplex_radius_ratio_triangle_right",
          "[simplex][radius_ratio][triangle]") {
    auto result = radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    double expected = 2.0 * std::numbers::sqrt2 - 2.0;
    CHECK_THAT(result, WithinAbs(expected, 1e-12));
}

TEST_CASE("simplex_radius_ratio_triangle_degenerate",
          "[simplex][radius_ratio][triangle]") {
    auto result = radius_ratio(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(result == 0.0);
}

// ============================================================================
// Radius Ratio — Tet
// ============================================================================
TEST_CASE("simplex_radius_ratio_tet_regular", "[simplex][radius_ratio][tet]") {
    auto result = radius_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("simplex_radius_ratio_tet_degenerate",
          "[simplex][radius_ratio][tet]") {
    auto result = radius_ratio(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK(result == 0.0);
}

TEST_CASE("simplex_radius_ratio_tet_right", "[simplex][radius_ratio][tet]") {
    auto result = radius_ratio(sp(rtet_a), sp(rtet_b), sp(rtet_c), sp(rtet_d));
    CHECK(result > 0.0);
    CHECK(result < 1.0);
}

// ============================================================================
// Min Angle — Triangle (radians)
// ============================================================================
TEST_CASE("simplex_min_angle_equilateral", "[simplex][min_angle][triangle]") {
    auto result = min_angle(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 3.0, 1e-10));
}

TEST_CASE("simplex_min_angle_right", "[simplex][min_angle][triangle]") {
    auto result = min_angle(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 4.0, 1e-10));
}

TEST_CASE("simplex_min_angle_degenerate", "[simplex][min_angle][triangle]") {
    auto result = min_angle(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(result, WithinAbs(0.0, 1e-10));
}

// ============================================================================
// Max Angle — Triangle (radians)
// ============================================================================
TEST_CASE("simplex_max_angle_equilateral", "[simplex][max_angle][triangle]") {
    auto result = max_angle(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 3.0, 1e-10));
}

TEST_CASE("simplex_max_angle_right", "[simplex][max_angle][triangle]") {
    auto result = max_angle(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 2.0, 1e-10));
}

TEST_CASE("simplex_max_angle_degenerate", "[simplex][max_angle][triangle]") {
    auto result = max_angle(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi, 1e-10));
}

// ============================================================================
// Dihedral Angles — Tet
// ============================================================================
TEST_CASE("simplex_tet_dihedral_angles_regular", "[simplex][dihedral][tet]") {
    auto angles =
        detail::tet_dihedral_angles(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Regular tet: all dihedral angles = arccos(1/3)
    double expected = std::acos(1.0 / 3.0);
    for (auto &a : angles) { CHECK_THAT(a, WithinAbs(expected, 1e-10)); }
}

// ============================================================================
// Min/Max Dihedral Angle — Tet (radians)
// ============================================================================
TEST_CASE("simplex_min_dihedral_angle_tet_regular",
          "[simplex][min_angle][tet]") {
    auto result =
        min_dihedral_angle(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    double expected = std::acos(1.0 / 3.0);
    CHECK_THAT(result, WithinAbs(expected, 1e-8));
}

TEST_CASE("simplex_max_dihedral_angle_tet_regular",
          "[simplex][max_angle][tet]") {
    auto result =
        max_dihedral_angle(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    double expected = std::acos(1.0 / 3.0);
    CHECK_THAT(result, WithinAbs(expected, 1e-8));
}

TEST_CASE("simplex_max_dihedral_angle_tet_right", "[simplex][max_angle][tet]") {
    auto result =
        max_dihedral_angle(sp(rtet_a), sp(rtet_b), sp(rtet_c), sp(rtet_d));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 2.0, 1e-8));
}

// ============================================================================
// Aspect Ratio — Triangle
// ============================================================================
TEST_CASE("simplex_aspect_ratio_triangle_equilateral",
          "[simplex][aspect_ratio][triangle]") {
    auto result = aspect_ratio(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("simplex_aspect_ratio_triangle_right",
          "[simplex][aspect_ratio][triangle]") {
    auto result = aspect_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(result > 1.0);
}

TEST_CASE("simplex_aspect_ratio_triangle_degenerate",
          "[simplex][aspect_ratio][triangle]") {
    auto result = aspect_ratio(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(result == std::numeric_limits<double>::infinity());
}

// ============================================================================
// Aspect Ratio — Tet
// ============================================================================
TEST_CASE("simplex_aspect_ratio_tet_regular", "[simplex][aspect_ratio][tet]") {
    auto result = aspect_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("simplex_aspect_ratio_tet_degenerate",
          "[simplex][aspect_ratio][tet]") {
    auto result = aspect_ratio(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK(result == std::numeric_limits<double>::infinity());
}

// ============================================================================
// Scaled Jacobian — Triangle
// ============================================================================
TEST_CASE("simplex_scaled_jacobian_triangle_equilateral",
          "[simplex][scaled_jacobian][triangle]") {
    auto result = scaled_jacobian(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("simplex_scaled_jacobian_triangle_right",
          "[simplex][scaled_jacobian][triangle]") {
    auto result = scaled_jacobian(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(result > 0.0);
    CHECK(result < 1.0);
}

TEST_CASE("simplex_scaled_jacobian_triangle_degenerate",
          "[simplex][scaled_jacobian][triangle]") {
    auto result = scaled_jacobian(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(result, WithinAbs(0.0, 1e-12));
}

// ============================================================================
// Scaled Jacobian — Tet
// ============================================================================
TEST_CASE("simplex_scaled_jacobian_tet_regular",
          "[simplex][scaled_jacobian][tet]") {
    auto result = scaled_jacobian(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("simplex_scaled_jacobian_tet_degenerate",
          "[simplex][scaled_jacobian][tet]") {
    auto result =
        scaled_jacobian(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK_THAT(result, WithinAbs(0.0, 1e-12));
}

TEST_CASE("simplex_scaled_jacobian_tet_inverted",
          "[simplex][scaled_jacobian][tet]") {
    // Swap c and d to invert
    auto result = scaled_jacobian(sp(tet_a), sp(tet_b), sp(tet_d), sp(tet_c));
    CHECK(result < 0.0);
}

// ============================================================================
// AMIPS — Triangle
// ============================================================================
TEST_CASE("simplex_amips_triangle_equilateral", "[simplex][amips][triangle]") {
    auto result = amips(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(2.0, 1e-12));
}

TEST_CASE("simplex_amips_triangle_right", "[simplex][amips][triangle]") {
    auto result = amips(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(result > 2.0);
}

TEST_CASE("simplex_amips_triangle_degenerate", "[simplex][amips][triangle]") {
    auto result = amips(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(result == std::numeric_limits<double>::infinity());
}

// ============================================================================
// AMIPS — Tet
// ============================================================================
TEST_CASE("simplex_amips_tet_regular", "[simplex][amips][tet]") {
    auto result = amips(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(3.0, 1e-10));
}

TEST_CASE("simplex_amips_tet_degenerate", "[simplex][amips][tet]") {
    auto result = amips(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK(result == std::numeric_limits<double>::infinity());
}

TEST_CASE("simplex_amips_tet_inverted", "[simplex][amips][tet]") {
    auto result = amips(sp(tet_a), sp(tet_b), sp(tet_d), sp(tet_c));
    CHECK(result == std::numeric_limits<double>::infinity());
}

// ============================================================================
// 3D embedded triangle tests
// ============================================================================
TEST_CASE("simplex_radius_ratio_triangle_3d",
          "[simplex][radius_ratio][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto result = radius_ratio(sp(a), sp(b), sp(c));
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("simplex_min_angle_3d", "[simplex][min_angle][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto result = min_angle(sp(a), sp(b), sp(c));
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 3.0, 1e-10));
}

// ============================================================================
// Float type tests
// ============================================================================
TEST_CASE("simplex_radius_ratio_triangle_float",
          "[simplex][radius_ratio][float]") {
    std::array<float, 2> a{0.0f, 0.0f};
    std::array<float, 2> b{1.0f, 0.0f};
    std::array<float, 2> c{0.5f, std::numbers::sqrt3_v<float> / 2.0f};
    auto result = radius_ratio(sp(a), sp(b), sp(c));
    static_assert(std::is_same_v<decltype(result), float>);
    CHECK_THAT(result, WithinAbs(1.0f, 1e-5f));
}

// ============================================================================
// Symmetry: permutation invariance
// ============================================================================
TEST_CASE("simplex_radius_ratio_triangle_permutation_invariant",
          "[simplex][radius_ratio][symmetry]") {
    auto r1 = radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r2 = radius_ratio(sp(rt_b), sp(rt_c), sp(rt_a));
    auto r3 = radius_ratio(sp(rt_c), sp(rt_a), sp(rt_b));
    CHECK_THAT(r1, WithinAbs(r2, 1e-14));
    CHECK_THAT(r1, WithinAbs(r3, 1e-14));
}

TEST_CASE("simplex_min_angle_permutation_invariant",
          "[simplex][min_angle][symmetry]") {
    auto r1 = min_angle(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r2 = min_angle(sp(rt_b), sp(rt_c), sp(rt_a));
    auto r3 = min_angle(sp(rt_c), sp(rt_a), sp(rt_b));
    CHECK_THAT(r1, WithinAbs(r2, 1e-14));
    CHECK_THAT(r1, WithinAbs(r3, 1e-14));
}

TEST_CASE("simplex_radius_ratio_tet_permutation_invariant",
          "[simplex][radius_ratio][tet][symmetry]") {
    auto r1 = radius_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    auto r2 = radius_ratio(sp(tet_b), sp(tet_c), sp(tet_d), sp(tet_a));
    auto r3 = radius_ratio(sp(tet_d), sp(tet_a), sp(tet_b), sp(tet_c));
    CHECK_THAT(r1, WithinAbs(r2, 1e-12));
    CHECK_THAT(r1, WithinAbs(r3, 1e-12));
}

// ============================================================================
// Scale invariance
// ============================================================================
TEST_CASE("simplex_radius_ratio_triangle_scale_invariant",
          "[simplex][radius_ratio][scale]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{10.0, 0.0};
    std::array<double, 2> c{0.0, 10.0};
    auto r1 = radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r2 = radius_ratio(sp(a), sp(b), sp(c));
    CHECK_THAT(r1, WithinAbs(r2, 1e-14));
}

TEST_CASE("simplex_aspect_ratio_triangle_scale_invariant",
          "[simplex][aspect_ratio][scale]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{100.0, 0.0};
    std::array<double, 2> c{0.0, 100.0};
    auto r1 = aspect_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r2 = aspect_ratio(sp(a), sp(b), sp(c));
    CHECK_THAT(r1, WithinAbs(r2, 1e-12));
}

// ============================================================================
// Layer 1.5M: Matrix overload tests
// ============================================================================

// Helper: build a D×N matrix from arrays (columns = vertices).
template <typename T, std::size_t D, std::size_t N>
auto make_matrix(const std::array<std::array<T, D>, N> &cols)
    -> ::zipper::Matrix<T, D, N> {
    ::zipper::Matrix<T, D, N> M;
    for (std::size_t j = 0; j < N; ++j) {
        for (std::size_t i = 0; i < D; ++i) { M(i, j) = cols[j][i]; }
    }
    return M;
}

// -- Circumcenter matrix overloads --

TEST_CASE("matrix_circumcenter_equilateral_2d",
          "[simplex][circumcenter][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto cc = circumcenter(M);
    auto cc_ref = circumcenter(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(cc(0), WithinAbs(cc_ref[0], 1e-12));
    CHECK_THAT(cc(1), WithinAbs(cc_ref[1], 1e-12));
}

TEST_CASE("matrix_circumcenter_with_radius_equilateral",
          "[simplex][circumcenter][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto [cc, r] = circumcenter_with_radius(M);
    auto [cc_ref, r_ref] =
        circumcenter_with_radius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(r, WithinAbs(r_ref, 1e-12));
}

TEST_CASE("matrix_circumcenter_with_squared_radius",
          "[simplex][circumcenter][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto [cc, r2] = circumcenter_with_squared_radius(M);
    auto [cc_ref, r2_ref] =
        circumcenter_with_squared_radius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(r2, WithinAbs(r2_ref, 1e-12));
}

TEST_CASE("matrix_circumcenter_edge_midpoint",
          "[simplex][circumcenter][matrix]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{4.0, 2.0};
    auto M = make_matrix<double, 2, 2>({a, b});
    auto cc = circumcenter(M);
    CHECK_THAT(cc(0), WithinAbs(2.0, 1e-14));
    CHECK_THAT(cc(1), WithinAbs(1.0, 1e-14));
}

TEST_CASE("matrix_circumcenter_tet", "[simplex][circumcenter][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto cc = circumcenter(M);
    auto cc_ref = circumcenter(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK_THAT(cc(i), WithinAbs(cc_ref[i], 1e-10));
    }
}

// -- Volume matrix overloads --

TEST_CASE("matrix_volume_signed_2d", "[simplex][volume][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = volume_signed(M);
    auto ref = volume_signed(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(ref, 1e-14));
}

TEST_CASE("matrix_volume_unsigned_3d", "[simplex][volume][matrix][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto M = make_matrix<double, 3, 3>({a, b, c});
    auto result = volume_unsigned(M);
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

// -- Min angle matrix overloads --

TEST_CASE("matrix_min_angle_equilateral", "[simplex][min_angle][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = min_angle(M);
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 3.0, 1e-10));
}

TEST_CASE("matrix_min_angle_right", "[simplex][min_angle][matrix]") {
    auto M = make_matrix<double, 2, 3>({rt_a, rt_b, rt_c});
    auto result = min_angle(M);
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 4.0, 1e-10));
}

TEST_CASE("matrix_min_dihedral_angle_tet",
          "[simplex][min_angle][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = min_dihedral_angle(M);
    double expected = std::acos(1.0 / 3.0);
    CHECK_THAT(result, WithinAbs(expected, 1e-8));
}

// -- Max angle matrix overloads --

TEST_CASE("matrix_max_angle_equilateral", "[simplex][max_angle][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = max_angle(M);
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 3.0, 1e-10));
}

TEST_CASE("matrix_max_angle_right", "[simplex][max_angle][matrix]") {
    auto M = make_matrix<double, 2, 3>({rt_a, rt_b, rt_c});
    auto result = max_angle(M);
    CHECK_THAT(result, WithinAbs(std::numbers::pi / 2.0, 1e-10));
}

TEST_CASE("matrix_max_dihedral_angle_tet",
          "[simplex][max_angle][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = max_dihedral_angle(M);
    double expected = std::acos(1.0 / 3.0);
    CHECK_THAT(result, WithinAbs(expected, 1e-8));
}

// -- Aspect ratio matrix overloads --

TEST_CASE("matrix_aspect_ratio_triangle_equilateral",
          "[simplex][aspect_ratio][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = aspect_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("matrix_aspect_ratio_tet_regular",
          "[simplex][aspect_ratio][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = aspect_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

// -- Radius ratio matrix overloads --

TEST_CASE("matrix_radius_ratio_triangle_equilateral",
          "[simplex][radius_ratio][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = radius_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("matrix_radius_ratio_tet_regular",
          "[simplex][radius_ratio][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = radius_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

// -- Scaled Jacobian matrix overloads --

TEST_CASE("matrix_scaled_jacobian_triangle_equilateral",
          "[simplex][scaled_jacobian][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = scaled_jacobian(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("matrix_scaled_jacobian_tet_regular",
          "[simplex][scaled_jacobian][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = scaled_jacobian(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

// -- AMIPS matrix overloads --

TEST_CASE("matrix_amips_triangle_equilateral", "[simplex][amips][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = amips(M);
    CHECK_THAT(result, WithinAbs(2.0, 1e-12));
}

TEST_CASE("matrix_amips_tet_regular", "[simplex][amips][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = amips(M);
    CHECK_THAT(result, WithinAbs(3.0, 1e-10));
}

// -- Point in circumsphere matrix overload --

TEST_CASE("matrix_point_in_circumsphere_inside",
          "[simplex][circumsphere][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    ::zipper::Vector<double, 2> centroid;
    centroid(0) = 0.5;
    centroid(1) = sqrt3 / 6.0;
    CHECK(point_in_circumsphere(centroid, M));
}

TEST_CASE("matrix_point_in_circumsphere_outside",
          "[simplex][circumsphere][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    ::zipper::Vector<double, 2> far_pt;
    far_pt(0) = 10.0;
    far_pt(1) = 10.0;
    CHECK_FALSE(point_in_circumsphere(far_pt, M));
}

TEST_CASE("matrix_point_in_circumsphere_tet",
          "[simplex][circumsphere][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    ::zipper::Vector<double, 3> centroid;
    centroid(0) = (tet_a[0] + tet_b[0] + tet_c[0] + tet_d[0]) / 4.0;
    centroid(1) = (tet_a[1] + tet_b[1] + tet_c[1] + tet_d[1]) / 4.0;
    centroid(2) = (tet_a[2] + tet_b[2] + tet_c[2] + tet_d[2]) / 4.0;
    CHECK(point_in_circumsphere(centroid, M));
}

// -- Cross-validation: matrix vs span results agree --

TEST_CASE("matrix_vs_span_agreement_all_metrics",
          "[simplex][matrix][agreement]") {
    auto M2 = make_matrix<double, 2, 3>({rt_a, rt_b, rt_c});
    auto M3 = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});

    SECTION("volume_signed") {
        CHECK_THAT(
            volume_signed(M2),
            WithinAbs(volume_signed(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-14));
    }
    SECTION("volume_unsigned") {
        CHECK_THAT(
            volume_unsigned(M2),
            WithinAbs(volume_unsigned(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-14));
    }
    SECTION("volume_signed_tet") {
        CHECK_THAT(
            volume_signed(M3),
            WithinAbs(volume_signed(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                      1e-12));
    }
    SECTION("min_angle") {
        CHECK_THAT(min_angle(M2),
                   WithinAbs(min_angle(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-10));
    }
    SECTION("max_angle") {
        CHECK_THAT(max_angle(M2),
                   WithinAbs(max_angle(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-10));
    }
    SECTION("aspect_ratio_triangle") {
        CHECK_THAT(
            aspect_ratio(M2),
            WithinAbs(aspect_ratio(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-12));
    }
    SECTION("radius_ratio_triangle") {
        CHECK_THAT(
            radius_ratio(M2),
            WithinAbs(radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-12));
    }
    SECTION("scaled_jacobian_triangle") {
        CHECK_THAT(
            scaled_jacobian(M2),
            WithinAbs(scaled_jacobian(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-12));
    }
    SECTION("amips_triangle") {
        CHECK_THAT(amips(M2),
                   WithinAbs(amips(sp(rt_a), sp(rt_b), sp(rt_c)), 1e-12));
    }
    SECTION("min_dihedral_angle") {
        CHECK_THAT(min_dihedral_angle(M3),
                   WithinAbs(min_dihedral_angle(
                                 sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                             1e-8));
    }
    SECTION("max_dihedral_angle") {
        CHECK_THAT(max_dihedral_angle(M3),
                   WithinAbs(max_dihedral_angle(
                                 sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                             1e-8));
    }
    SECTION("aspect_ratio_tet") {
        CHECK_THAT(
            aspect_ratio(M3),
            WithinAbs(aspect_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                      1e-10));
    }
    SECTION("radius_ratio_tet") {
        CHECK_THAT(
            radius_ratio(M3),
            WithinAbs(radius_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                      1e-10));
    }
    SECTION("scaled_jacobian_tet") {
        CHECK_THAT(scaled_jacobian(M3),
                   WithinAbs(scaled_jacobian(
                                 sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                             1e-10));
    }
    SECTION("amips_tet") {
        CHECK_THAT(amips(M3),
                   WithinAbs(amips(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d)),
                             1e-10));
    }
}

// ============================================================================
// Dynamic-extent matrix tests (D×dynamic columns)
// ============================================================================

// Helper: build a D×dynamic matrix from arrays (columns = vertices).
template <typename T, std::size_t D, std::size_t N>
auto make_dynamic_matrix(const std::array<std::array<T, D>, N> &cols)
    -> ::zipper::Matrix<T, D, std::dynamic_extent> {
    ::zipper::Matrix<T, D, std::dynamic_extent> M(N);
    for (std::size_t j = 0; j < N; ++j) {
        for (std::size_t i = 0; i < D; ++i) { M(i, j) = cols[j][i]; }
    }
    return M;
}

TEST_CASE("dynamic_circumcenter_equilateral_2d",
          "[simplex][circumcenter][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto cc = circumcenter(M);
    auto cc_ref = circumcenter(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(cc(0), WithinAbs(cc_ref[0], 1e-12));
    CHECK_THAT(cc(1), WithinAbs(cc_ref[1], 1e-12));
}

TEST_CASE("dynamic_circumcenter_right_2d", "[simplex][circumcenter][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({rt_a, rt_b, rt_c});
    auto cc = circumcenter(M);
    CHECK_THAT(cc(0), WithinAbs(0.5, 1e-12));
    CHECK_THAT(cc(1), WithinAbs(0.5, 1e-12));
}

TEST_CASE("dynamic_circumcenter_edge_midpoint",
          "[simplex][circumcenter][dynamic]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{4.0, 2.0};
    auto M = make_dynamic_matrix<double, 2, 2>({a, b});
    auto cc = circumcenter(M);
    CHECK_THAT(cc(0), WithinAbs(2.0, 1e-14));
    CHECK_THAT(cc(1), WithinAbs(1.0, 1e-14));
}

TEST_CASE("dynamic_circumcenter_tet", "[simplex][circumcenter][dynamic][tet]") {
    auto M = make_dynamic_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto cc = circumcenter(M);
    auto cc_ref = circumcenter(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK_THAT(cc(i), WithinAbs(cc_ref[i], 1e-10));
    }
}

TEST_CASE("dynamic_circumcenter_with_radius",
          "[simplex][circumcenter][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto [cc, r] = circumcenter_with_radius(M);
    auto [cc_ref, r_ref] =
        circumcenter_with_radius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(r, WithinAbs(r_ref, 1e-12));
}

TEST_CASE("dynamic_circumcenter_with_squared_radius",
          "[simplex][circumcenter][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto [cc, r2] = circumcenter_with_squared_radius(M);
    auto [cc_ref, r2_ref] =
        circumcenter_with_squared_radius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(r2, WithinAbs(r2_ref, 1e-12));
}

TEST_CASE("dynamic_volume_signed_triangle_2d", "[simplex][volume][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = volume_signed(M);
    auto ref = volume_signed(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(result, WithinAbs(ref, 1e-14));
}

TEST_CASE("dynamic_volume_unsigned_triangle_3d",
          "[simplex][volume][dynamic][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto M = make_dynamic_matrix<double, 3, 3>({a, b, c});
    auto result = volume_unsigned(M);
    CHECK_THAT(result, WithinAbs(sqrt3 / 4.0, 1e-14));
}

TEST_CASE("dynamic_volume_signed_tet", "[simplex][volume][dynamic][tet]") {
    auto M = make_dynamic_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = volume_signed(M);
    auto ref = volume_signed(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(ref, 1e-12));
}

TEST_CASE("dynamic_point_in_circumsphere", "[simplex][circumsphere][dynamic]") {
    auto M = make_dynamic_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    ::zipper::Vector<double, 2> centroid;
    centroid(0) = 0.5;
    centroid(1) = sqrt3 / 6.0;
    CHECK(point_in_circumsphere(centroid, M));

    ::zipper::Vector<double, 2> far_pt;
    far_pt(0) = 10.0;
    far_pt(1) = 10.0;
    CHECK_FALSE(point_in_circumsphere(far_pt, M));
}

// ============================================================================
// Edge Matrix — Triangle 2D
// ============================================================================
TEST_CASE("edge_matrix_equilateral_2d", "[simplex][edge_matrix][triangle]") {
    auto E = edge_matrix(sp(eq_a), sp(eq_b), sp(eq_c));
    // E = [b-a | c-a] = [[1, 0.5], [0, sqrt3/2]]
    CHECK_THAT(E(0, 0), WithinAbs(1.0, 1e-14));
    CHECK_THAT(E(1, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(E(0, 1), WithinAbs(0.5, 1e-14));
    CHECK_THAT(E(1, 1), WithinAbs(sqrt3 / 2.0, 1e-14));
}

TEST_CASE("edge_matrix_right_2d", "[simplex][edge_matrix][triangle]") {
    auto E = edge_matrix(sp(rt_a), sp(rt_b), sp(rt_c));
    // E = [b-a | c-a] = [[1, 0], [0, 1]]
    CHECK_THAT(E(0, 0), WithinAbs(1.0, 1e-14));
    CHECK_THAT(E(1, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(E(0, 1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(E(1, 1), WithinAbs(1.0, 1e-14));
}

TEST_CASE("edge_matrix_tet_regular", "[simplex][edge_matrix][tet]") {
    auto E = edge_matrix(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // 3×3 matrix, columns are b-a, c-a, d-a
    CHECK_THAT(E(0, 0), WithinAbs(1.0, 1e-14)); // b-a = (1,0,0)
    CHECK_THAT(E(1, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(E(2, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(E(0, 1), WithinAbs(0.5, 1e-14)); // c-a = (0.5, sqrt3/2, 0)
    CHECK_THAT(E(1, 1), WithinAbs(sqrt3 / 2.0, 1e-14));
    CHECK_THAT(E(2, 1), WithinAbs(0.0, 1e-14));
}

TEST_CASE("edge_matrix_matrix_overload_2d", "[simplex][edge_matrix][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto E = edge_matrix(M);
    auto E_ref = edge_matrix(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(E(0, 0), WithinAbs(E_ref(0, 0), 1e-14));
    CHECK_THAT(E(0, 1), WithinAbs(E_ref(0, 1), 1e-14));
    CHECK_THAT(E(1, 0), WithinAbs(E_ref(1, 0), 1e-14));
    CHECK_THAT(E(1, 1), WithinAbs(E_ref(1, 1), 1e-14));
}

TEST_CASE("edge_matrix_matrix_overload_tet",
          "[simplex][edge_matrix][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto E = edge_matrix(M);
    auto E_ref = edge_matrix(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            CHECK_THAT(E(i, j), WithinAbs(E_ref(i, j), 1e-14));
        }
    }
}

TEST_CASE("edge_matrix_edge_1d", "[simplex][edge_matrix]") {
    // Edge: 2 vertices in 2D
    auto E = edge_matrix(sp(eq_a), sp(eq_b));
    // E = [b-a] = [[1], [0]]
    CHECK_THAT(E(0, 0), WithinAbs(1.0, 1e-14));
    CHECK_THAT(E(1, 0), WithinAbs(0.0, 1e-14));
}

// ============================================================================
// Edge Length
// ============================================================================
TEST_CASE("edge_length_unit_2d", "[simplex][edge_length]") {
    auto result = edge_length(sp(eq_a), sp(eq_b));
    CHECK_THAT(result, WithinAbs(1.0, 1e-14));
}

TEST_CASE("edge_length_diagonal_2d", "[simplex][edge_length]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{3.0, 4.0};
    auto result = edge_length(sp(a), sp(b));
    CHECK_THAT(result, WithinAbs(5.0, 1e-14));
}

TEST_CASE("edge_length_3d", "[simplex][edge_length][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 2.0, 2.0};
    auto result = edge_length(sp(a), sp(b));
    CHECK_THAT(result, WithinAbs(3.0, 1e-14));
}

TEST_CASE("edge_length_zero", "[simplex][edge_length]") {
    auto result = edge_length(sp(eq_a), sp(eq_a));
    CHECK_THAT(result, WithinAbs(0.0, 1e-14));
}

TEST_CASE("edge_length_equals_volume_unsigned", "[simplex][edge_length]") {
    // edge_length(a, b) == volume_unsigned(a, b)
    auto el = edge_length(sp(eq_a), sp(eq_b));
    auto vu = volume_unsigned(sp(eq_a), sp(eq_b));
    CHECK_THAT(el, WithinAbs(vu, 1e-14));
}

// ============================================================================
// Max Edge Length
// ============================================================================
TEST_CASE("max_edge_length_equilateral_2d",
          "[simplex][max_edge_length][triangle]") {
    auto result = max_edge_length(sp(eq_a), sp(eq_b), sp(eq_c));
    // Equilateral: all edges = 1
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("max_edge_length_right_2d", "[simplex][max_edge_length][triangle]") {
    auto result = max_edge_length(sp(rt_a), sp(rt_b), sp(rt_c));
    // Right triangle: hypotenuse = sqrt(2)
    CHECK_THAT(result, WithinAbs(std::numbers::sqrt2, 1e-12));
}

TEST_CASE("max_edge_length_regular_tet", "[simplex][max_edge_length][tet]") {
    auto result = max_edge_length(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Regular tet: all edges = 1
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("max_edge_length_right_tet", "[simplex][max_edge_length][tet]") {
    auto result =
        max_edge_length(sp(rtet_a), sp(rtet_b), sp(rtet_c), sp(rtet_d));
    // Right tet: longest edges are the face diagonals = sqrt(2)
    CHECK_THAT(result, WithinAbs(std::numbers::sqrt2, 1e-12));
}

TEST_CASE("max_edge_length_matrix_2d", "[simplex][max_edge_length][matrix]") {
    auto M = make_matrix<double, 2, 3>({rt_a, rt_b, rt_c});
    auto result = max_edge_length(M);
    auto ref = max_edge_length(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(result, WithinAbs(ref, 1e-14));
}

TEST_CASE("max_edge_length_matrix_tet",
          "[simplex][max_edge_length][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = max_edge_length(M);
    auto ref = max_edge_length(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(result, WithinAbs(ref, 1e-12));
}

// ============================================================================
// Circumradius
// ============================================================================
TEST_CASE("circumradius_equilateral_2d", "[simplex][circumradius][triangle]") {
    auto R = circumradius(sp(eq_a), sp(eq_b), sp(eq_c));
    // Circumradius of equilateral triangle with side 1: R = 1/sqrt(3)
    CHECK_THAT(R, WithinAbs(1.0 / sqrt3, 1e-12));
}

TEST_CASE("circumradius_right_2d", "[simplex][circumradius][triangle]") {
    auto R = circumradius(sp(rt_a), sp(rt_b), sp(rt_c));
    // Right triangle: R = hypotenuse/2 = sqrt(2)/2
    CHECK_THAT(R, WithinAbs(std::numbers::sqrt2 / 2.0, 1e-12));
}

TEST_CASE("circumradius_squared_equilateral",
          "[simplex][circumradius][triangle]") {
    auto R2 = circumradius_squared(sp(eq_a), sp(eq_b), sp(eq_c));
    auto R = circumradius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(R2, WithinAbs(R * R, 1e-12));
}

TEST_CASE("circumradius_regular_tet", "[simplex][circumradius][tet]") {
    auto R = circumradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Regular tet with edge 1: R = sqrt(6)/4
    double expected = sqrt6 / 4.0;
    CHECK_THAT(R, WithinAbs(expected, 1e-10));
}

TEST_CASE("circumradius_edge", "[simplex][circumradius]") {
    // Circumradius of an edge = half the length
    auto R = circumradius(sp(eq_a), sp(eq_b));
    CHECK_THAT(R, WithinAbs(0.5, 1e-14));
}

TEST_CASE("circumradius_matrix_2d", "[simplex][circumradius][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto R = circumradius(M);
    auto R_ref = circumradius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(R, WithinAbs(R_ref, 1e-12));
}

TEST_CASE("circumradius_squared_matrix_tet",
          "[simplex][circumradius][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto R2 = circumradius_squared(M);
    auto R2_ref =
        circumradius_squared(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(R2, WithinAbs(R2_ref, 1e-10));
}

TEST_CASE("circumradius_agrees_with_circumcenter_with_radius",
          "[simplex][circumradius][triangle]") {
    auto R = circumradius(sp(rt_a), sp(rt_b), sp(rt_c));
    auto [cc, r] = circumcenter_with_radius(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(R, WithinAbs(r, 1e-14));
}

// ============================================================================
// Boundary Volume
// ============================================================================
TEST_CASE("boundary_volume_equilateral_2d",
          "[simplex][boundary_volume][triangle]") {
    auto bv = boundary_volume(sp(eq_a), sp(eq_b), sp(eq_c));
    // Perimeter of equilateral triangle with side 1 = 3.0
    CHECK_THAT(bv, WithinAbs(3.0, 1e-12));
}

TEST_CASE("boundary_volume_right_2d", "[simplex][boundary_volume][triangle]") {
    auto bv = boundary_volume(sp(rt_a), sp(rt_b), sp(rt_c));
    // Perimeter: 1 + 1 + sqrt(2)
    double expected = 2.0 + std::numbers::sqrt2;
    CHECK_THAT(bv, WithinAbs(expected, 1e-12));
}

TEST_CASE("boundary_volume_regular_tet", "[simplex][boundary_volume][tet]") {
    auto bv = boundary_volume(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Surface area of regular tet with edge 1: 4 * (sqrt(3)/4) = sqrt(3)
    CHECK_THAT(bv, WithinAbs(sqrt3, 1e-10));
}

TEST_CASE("boundary_volume_degenerate_2d",
          "[simplex][boundary_volume][triangle]") {
    auto bv = boundary_volume(sp(dg_a), sp(dg_b), sp(dg_c));
    // Degenerate: "perimeter" = |0,2| + |0,1| + |1,2| = 2 + 1 + 1 = 4
    CHECK_THAT(bv, WithinAbs(4.0, 1e-14));
}

TEST_CASE("boundary_volume_matrix_2d", "[simplex][boundary_volume][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto bv = boundary_volume(M);
    auto bv_ref = boundary_volume(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(bv, WithinAbs(bv_ref, 1e-12));
}

TEST_CASE("boundary_volume_matrix_tet",
          "[simplex][boundary_volume][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto bv = boundary_volume(M);
    auto bv_ref = boundary_volume(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(bv, WithinAbs(bv_ref, 1e-10));
}

// ============================================================================
// Inradius
// ============================================================================
TEST_CASE("inradius_equilateral_2d", "[simplex][inradius][triangle]") {
    auto r = inradius(sp(eq_a), sp(eq_b), sp(eq_c));
    // Inradius of equilateral triangle with side 1: r = 1/(2*sqrt(3))
    double expected = 1.0 / (2.0 * sqrt3);
    CHECK_THAT(r, WithinAbs(expected, 1e-12));
}

TEST_CASE("inradius_right_2d", "[simplex][inradius][triangle]") {
    auto r = inradius(sp(rt_a), sp(rt_b), sp(rt_c));
    // Inradius = 2 * area / perimeter = 2 * 0.5 / (2 + sqrt(2))
    //          = 1 / (2 + sqrt(2))
    double expected = 1.0 / (2.0 + std::numbers::sqrt2);
    CHECK_THAT(r, WithinAbs(expected, 1e-12));
}

TEST_CASE("inradius_degenerate_2d", "[simplex][inradius][triangle]") {
    auto r = inradius(sp(dg_a), sp(dg_b), sp(dg_c));
    // Degenerate: area = 0, so inradius = 0
    CHECK_THAT(r, WithinAbs(0.0, 1e-14));
}

TEST_CASE("inradius_regular_tet", "[simplex][inradius][tet]") {
    auto r = inradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Regular tet with edge 1: r = 1/(2*sqrt(6))
    double expected = 1.0 / (2.0 * sqrt6);
    CHECK_THAT(r, WithinAbs(expected, 1e-10));
}

TEST_CASE("inradius_degenerate_tet", "[simplex][inradius][tet]") {
    auto r = inradius(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK_THAT(r, WithinAbs(0.0, 1e-14));
}

TEST_CASE("inradius_matrix_2d", "[simplex][inradius][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto r = inradius(M);
    auto r_ref = inradius(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(r, WithinAbs(r_ref, 1e-12));
}

TEST_CASE("inradius_matrix_tet", "[simplex][inradius][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto r = inradius(M);
    auto r_ref = inradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(r, WithinAbs(r_ref, 1e-10));
}

// ============================================================================
// Jacobian — Reference simplex inverse
// ============================================================================
TEST_CASE("reference_simplex_inverse_triangle", "[simplex][jacobian]") {
    auto inv_R = reference_simplex_inverse<2>();
    // inv(R) = [[1, -1/sqrt(3)], [0, 2/sqrt(3)]]
    double inv_sqrt3 = 1.0 / sqrt3;
    CHECK_THAT(inv_R(0, 0), WithinAbs(1.0, 1e-14));
    CHECK_THAT(inv_R(0, 1), WithinAbs(-inv_sqrt3, 1e-14));
    CHECK_THAT(inv_R(1, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(inv_R(1, 1), WithinAbs(2.0 * inv_sqrt3, 1e-14));
}

TEST_CASE("reference_simplex_inverse_tet", "[simplex][jacobian]") {
    auto inv_R = reference_simplex_inverse<3>();
    double inv_sqrt3 = 1.0 / sqrt3;
    double inv_sqrt6 = 1.0 / sqrt6;
    CHECK_THAT(inv_R(0, 0), WithinAbs(1.0, 1e-14));
    CHECK_THAT(inv_R(0, 1), WithinAbs(-inv_sqrt3, 1e-14));
    CHECK_THAT(inv_R(0, 2), WithinAbs(-inv_sqrt6, 1e-14));
    CHECK_THAT(inv_R(1, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(inv_R(1, 1), WithinAbs(2.0 / sqrt3, 1e-14));
    CHECK_THAT(inv_R(1, 2), WithinAbs(-inv_sqrt6, 1e-14));
    CHECK_THAT(inv_R(2, 0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(inv_R(2, 1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(inv_R(2, 2), WithinAbs(3.0 / sqrt6, 1e-14));
}

// ============================================================================
// Jacobian — Triangle and Tet
// ============================================================================
TEST_CASE("jacobian_equilateral_triangle_is_identity",
          "[simplex][jacobian][triangle]") {
    // For an equilateral triangle, J = E * inv(R) should be a rotation/isometry
    // (det(J) should be 1 since our reference has the same area)
    auto J = jacobian(sp(eq_a), sp(eq_b), sp(eq_c));
    // J should have det = 1 (equilateral maps to equilateral reference)
    double det = J(0, 0) * J(1, 1) - J(0, 1) * J(1, 0);
    CHECK_THAT(det, WithinAbs(1.0, 1e-12));
}

TEST_CASE("jacobian_right_triangle_det_positive",
          "[simplex][jacobian][triangle]") {
    auto J = jacobian(sp(rt_a), sp(rt_b), sp(rt_c));
    double det = J(0, 0) * J(1, 1) - J(0, 1) * J(1, 0);
    CHECK(det > 0.0);
}

TEST_CASE("jacobian_regular_tet_det_one", "[simplex][jacobian][tet]") {
    auto J = jacobian(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    // Build 3x3 matrix for determinant
    ::zipper::Matrix<double, 3, 3> Jmat;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) Jmat(i, j) = J(i, j);
    double det = ::zipper::utils::determinant(Jmat);
    CHECK_THAT(det, WithinAbs(1.0, 1e-10));
}

TEST_CASE("jacobian_matrix_overload_triangle", "[simplex][jacobian][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto J = jacobian(M);
    auto J_ref = jacobian(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(J(0, 0), WithinAbs(J_ref(0, 0), 1e-12));
    CHECK_THAT(J(0, 1), WithinAbs(J_ref(0, 1), 1e-12));
    CHECK_THAT(J(1, 0), WithinAbs(J_ref(1, 0), 1e-12));
    CHECK_THAT(J(1, 1), WithinAbs(J_ref(1, 1), 1e-12));
}

TEST_CASE("jacobian_custom_reference", "[simplex][jacobian]") {
    // Test J = E * ref_inv with a custom reference
    auto E = edge_matrix(sp(eq_a), sp(eq_b), sp(eq_c));
    auto inv_R = reference_simplex_inverse<2>();
    auto J = jacobian(E, inv_R);
    auto J_ref = jacobian(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(J(0, 0), WithinAbs(J_ref(0, 0), 1e-12));
    CHECK_THAT(J(0, 1), WithinAbs(J_ref(0, 1), 1e-12));
    CHECK_THAT(J(1, 0), WithinAbs(J_ref(1, 0), 1e-12));
    CHECK_THAT(J(1, 1), WithinAbs(J_ref(1, 1), 1e-12));
}

// ============================================================================
// Generic (unsuffixed) overloads — cross-validation with deprecated variants
// ============================================================================
TEST_CASE("generic_radius_ratio_triangle", "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_radius_ratio_right_triangle",
          "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    double expected = 2.0 * std::numbers::sqrt2 - 2.0;
    CHECK_THAT(generic, WithinAbs(expected, 1e-12));
}

TEST_CASE("generic_radius_ratio_degenerate",
          "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(generic == 0.0);
}

TEST_CASE("generic_radius_ratio_tet", "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_radius_ratio_tet_degenerate",
          "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK(generic == 0.0);
}

TEST_CASE("generic_radius_ratio_tet_right",
          "[simplex][radius_ratio][generic]") {
    auto generic = radius_ratio(sp(rtet_a), sp(rtet_b), sp(rtet_c), sp(rtet_d));
    CHECK(generic > 0.0);
    CHECK(generic < 1.0);
}

TEST_CASE("generic_aspect_ratio_triangle", "[simplex][aspect_ratio][generic]") {
    auto generic = aspect_ratio(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_aspect_ratio_right_triangle",
          "[simplex][aspect_ratio][generic]") {
    auto generic = aspect_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(generic > 1.0);
}

TEST_CASE("generic_aspect_ratio_degenerate",
          "[simplex][aspect_ratio][generic]") {
    auto generic = aspect_ratio(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(generic == std::numeric_limits<double>::infinity());
}

TEST_CASE("generic_aspect_ratio_tet", "[simplex][aspect_ratio][generic]") {
    auto generic = aspect_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_aspect_ratio_tet_degenerate",
          "[simplex][aspect_ratio][generic]") {
    auto generic = aspect_ratio(sp(dtet_a), sp(dtet_b), sp(dtet_c), sp(dtet_d));
    CHECK(generic == std::numeric_limits<double>::infinity());
}

TEST_CASE("generic_scaled_jacobian_triangle",
          "[simplex][scaled_jacobian][generic]") {
    auto generic = scaled_jacobian(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_scaled_jacobian_right_triangle",
          "[simplex][scaled_jacobian][generic]") {
    auto generic = scaled_jacobian(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(generic > 0.0);
    CHECK(generic < 1.0);
}

TEST_CASE("generic_scaled_jacobian_degenerate",
          "[simplex][scaled_jacobian][generic]") {
    auto generic = scaled_jacobian(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(generic, WithinAbs(0.0, 1e-12));
}

TEST_CASE("generic_scaled_jacobian_tet",
          "[simplex][scaled_jacobian][generic]") {
    auto generic = scaled_jacobian(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(generic, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_scaled_jacobian_tet_inverted",
          "[simplex][scaled_jacobian][generic]") {
    auto generic = scaled_jacobian(sp(tet_a), sp(tet_b), sp(tet_d), sp(tet_c));
    CHECK(generic < 0.0);
}

TEST_CASE("generic_amips_triangle", "[simplex][amips][generic]") {
    auto generic = amips(sp(eq_a), sp(eq_b), sp(eq_c));
    CHECK_THAT(generic, WithinAbs(2.0, 1e-12));
}

TEST_CASE("generic_amips_right_triangle", "[simplex][amips][generic]") {
    auto generic = amips(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK(generic > 2.0);
}

TEST_CASE("generic_amips_degenerate", "[simplex][amips][generic]") {
    auto generic = amips(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK(generic == std::numeric_limits<double>::infinity());
}

TEST_CASE("generic_amips_tet", "[simplex][amips][generic]") {
    auto generic = amips(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    CHECK_THAT(generic, WithinAbs(3.0, 1e-10));
}

TEST_CASE("generic_amips_tet_inverted", "[simplex][amips][generic]") {
    auto generic = amips(sp(tet_a), sp(tet_b), sp(tet_d), sp(tet_c));
    CHECK(generic == std::numeric_limits<double>::infinity());
}

// ============================================================================
// Generic matrix overloads for new primitives
// ============================================================================
TEST_CASE("generic_radius_ratio_matrix_2d",
          "[simplex][radius_ratio][generic][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = radius_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_radius_ratio_matrix_tet",
          "[simplex][radius_ratio][generic][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = radius_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_aspect_ratio_matrix_2d",
          "[simplex][aspect_ratio][generic][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = aspect_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_aspect_ratio_matrix_tet",
          "[simplex][aspect_ratio][generic][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = aspect_ratio(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_scaled_jacobian_matrix_2d",
          "[simplex][scaled_jacobian][generic][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = scaled_jacobian(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-12));
}

TEST_CASE("generic_scaled_jacobian_matrix_tet",
          "[simplex][scaled_jacobian][generic][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = scaled_jacobian(M);
    CHECK_THAT(result, WithinAbs(1.0, 1e-10));
}

TEST_CASE("generic_amips_matrix_2d", "[simplex][amips][generic][matrix]") {
    auto M = make_matrix<double, 2, 3>({eq_a, eq_b, eq_c});
    auto result = amips(M);
    CHECK_THAT(result, WithinAbs(2.0, 1e-12));
}

TEST_CASE("generic_amips_matrix_tet",
          "[simplex][amips][generic][matrix][tet]") {
    auto M = make_matrix<double, 3, 4>({tet_a, tet_b, tet_c, tet_d});
    auto result = amips(M);
    CHECK_THAT(result, WithinAbs(3.0, 1e-10));
}

// ============================================================================
// Composition verification: radius_ratio = K * inradius / circumradius
// ============================================================================
TEST_CASE("radius_ratio_composition_triangle",
          "[simplex][radius_ratio][composition]") {
    // Verify: radius_ratio = K * inradius / circumradius for K=2
    auto rr = radius_ratio(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r = inradius(sp(rt_a), sp(rt_b), sp(rt_c));
    auto R = circumradius(sp(rt_a), sp(rt_b), sp(rt_c));
    double composed = 2.0 * r / R;
    CHECK_THAT(rr, WithinAbs(composed, 1e-12));
}

TEST_CASE("radius_ratio_composition_tet",
          "[simplex][radius_ratio][composition]") {
    // Verify: radius_ratio = K * inradius / circumradius for K=3
    auto rr = radius_ratio(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    auto r = inradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    auto R = circumradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    double composed = 3.0 * r / R;
    CHECK_THAT(rr, WithinAbs(composed, 1e-10));
}

// ============================================================================
// Composition verification: inradius = K * volume / boundary_volume
// ============================================================================
TEST_CASE("inradius_composition_triangle", "[simplex][inradius][composition]") {
    auto r = inradius(sp(rt_a), sp(rt_b), sp(rt_c));
    auto vol = volume_unsigned(sp(rt_a), sp(rt_b), sp(rt_c));
    auto bvol = boundary_volume(sp(rt_a), sp(rt_b), sp(rt_c));
    double composed = 2.0 * vol / bvol;
    CHECK_THAT(r, WithinAbs(composed, 1e-12));
}

TEST_CASE("inradius_composition_tet", "[simplex][inradius][composition]") {
    auto r = inradius(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    auto vol = volume_unsigned(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    auto bvol = boundary_volume(sp(tet_a), sp(tet_b), sp(tet_c), sp(tet_d));
    double composed = 3.0 * vol / bvol;
    CHECK_THAT(r, WithinAbs(composed, 1e-10));
}

// ============================================================================
// 3D embedded triangle tests for new primitives
// ============================================================================
TEST_CASE("circumradius_triangle_3d", "[simplex][circumradius][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto R = circumradius(sp(a), sp(b), sp(c));
    CHECK_THAT(R, WithinAbs(1.0 / sqrt3, 1e-12));
}

TEST_CASE("boundary_volume_triangle_3d",
          "[simplex][boundary_volume][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto bv = boundary_volume(sp(a), sp(b), sp(c));
    CHECK_THAT(bv, WithinAbs(3.0, 1e-12));
}

TEST_CASE("inradius_triangle_3d", "[simplex][inradius][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto r = inradius(sp(a), sp(b), sp(c));
    CHECK_THAT(r, WithinAbs(1.0 / (2.0 * sqrt3), 1e-12));
}

// ============================================================================
// Scale invariance for new primitives
// ============================================================================
TEST_CASE("inradius_scales_linearly", "[simplex][inradius][scale]") {
    // Inradius should scale linearly with size
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{10.0, 0.0};
    std::array<double, 2> c{0.0, 10.0};
    auto r1 = inradius(sp(rt_a), sp(rt_b), sp(rt_c));
    auto r10 = inradius(sp(a), sp(b), sp(c));
    CHECK_THAT(r10, WithinAbs(10.0 * r1, 1e-12));
}

TEST_CASE("circumradius_scales_linearly", "[simplex][circumradius][scale]") {
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{10.0, 0.0};
    std::array<double, 2> c{0.0, 10.0};
    auto R1 = circumradius(sp(rt_a), sp(rt_b), sp(rt_c));
    auto R10 = circumradius(sp(a), sp(b), sp(c));
    CHECK_THAT(R10, WithinAbs(10.0 * R1, 1e-12));
}

TEST_CASE("boundary_volume_scales_with_dimension",
          "[simplex][boundary_volume][scale]") {
    // For a triangle (perimeter), boundary_volume scales linearly
    std::array<double, 2> a{0.0, 0.0};
    std::array<double, 2> b{10.0, 0.0};
    std::array<double, 2> c{0.0, 10.0};
    auto bv1 = boundary_volume(sp(rt_a), sp(rt_b), sp(rt_c));
    auto bv10 = boundary_volume(sp(a), sp(b), sp(c));
    CHECK_THAT(bv10, WithinAbs(10.0 * bv1, 1e-12));
}

// ============================================================================
// Face Normal — Layer 1 (zipper-concepts core, 3D)
// ============================================================================

TEST_CASE("face_normal_right_triangle_3d",
          "[simplex][face_normal][triangle][3d]") {
    // Right triangle in XY plane: (0,0,0), (1,0,0), (0,1,0).
    // Cross product (1,0,0)x(0,1,0) = (0,0,1).
    ::zipper::Vector<double, 3> v0, v1, v2;
    v0(0) = 0;
    v0(1) = 0;
    v0(2) = 0;
    v1(0) = 1;
    v1(1) = 0;
    v1(2) = 0;
    v2(0) = 0;
    v2(1) = 1;
    v2(2) = 0;
    auto n = face_normal(v0, v1, v2);
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(1.0, 1e-14));
    // Magnitude = 2 * area = 2 * 0.5 = 1.0
    CHECK_THAT(n.norm(), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_normal_equilateral_3d",
          "[simplex][face_normal][triangle][3d]") {
    // Equilateral triangle in XY plane, side 1.
    ::zipper::Vector<double, 3> v0, v1, v2;
    v0(0) = 0;
    v0(1) = 0;
    v0(2) = 0;
    v1(0) = 1;
    v1(1) = 0;
    v1(2) = 0;
    v2(0) = 0.5;
    v2(1) = sqrt3 / 2.0;
    v2(2) = 0;
    auto n = face_normal(v0, v1, v2);
    // Normal should point in +z.
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK(n(2) > 0.0);
    // Magnitude = 2 * area = 2 * sqrt(3)/4 = sqrt(3)/2
    CHECK_THAT(n.norm(), WithinAbs(sqrt3 / 2.0, 1e-14));
}

TEST_CASE("face_normal_orientation_3d",
          "[simplex][face_normal][triangle][3d]") {
    // Reversing vertex order flips the normal.
    ::zipper::Vector<double, 3> v0, v1, v2;
    v0(0) = 0;
    v0(1) = 0;
    v0(2) = 0;
    v1(0) = 1;
    v1(1) = 0;
    v1(2) = 0;
    v2(0) = 0;
    v2(1) = 1;
    v2(2) = 0;
    auto n_pos = face_normal(v0, v1, v2);
    auto n_neg = face_normal(v0, v2, v1);
    for (std::size_t d = 0; d < 3; ++d) {
        CHECK_THAT(n_pos(d), WithinAbs(-n_neg(d), 1e-14));
    }
}

TEST_CASE("face_normal_magnitude_is_twice_area_3d",
          "[simplex][face_normal][triangle][3d]") {
    // Triangle: (0,0,0), (3,0,0), (0,4,0). Area = 6.0, mag = 12.0.
    ::zipper::Vector<double, 3> v0, v1, v2;
    v0(0) = 0;
    v0(1) = 0;
    v0(2) = 0;
    v1(0) = 3;
    v1(1) = 0;
    v1(2) = 0;
    v2(0) = 0;
    v2(1) = 4;
    v2(2) = 0;
    auto n = face_normal(v0, v1, v2);
    CHECK_THAT(n.norm(), WithinAbs(12.0, 1e-14));
}

TEST_CASE("face_normal_tilted_triangle",
          "[simplex][face_normal][triangle][3d]") {
    // Triangle not in XY plane: (0,0,0), (1,0,0), (0,0,1).
    // Edges: e1=(1,0,0), e2=(0,0,1). Cross = (0,-1,0).
    ::zipper::Vector<double, 3> v0, v1, v2;
    v0(0) = 0;
    v0(1) = 0;
    v0(2) = 0;
    v1(0) = 1;
    v1(1) = 0;
    v1(2) = 0;
    v2(0) = 0;
    v2(1) = 0;
    v2(2) = 1;
    auto n = face_normal(v0, v1, v2);
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(-1.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(0.0, 1e-14));
}

// ============================================================================
// Face Normal — Layer 1.5 (PointSpan, 3D)
// ============================================================================

TEST_CASE("face_normal_pointspan_right_3d",
          "[simplex][face_normal][pointspan][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.0, 1.0, 0.0};
    auto n = face_normal(sp(a), sp(b), sp(c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_normal_pointspan_equilateral_3d",
          "[simplex][face_normal][pointspan][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.5, sqrt3 / 2.0, 0.0};
    auto n = face_normal(sp(a), sp(b), sp(c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n.norm(), WithinAbs(sqrt3 / 2.0, 1e-14));
}

// ============================================================================
// Face Normal — Layer 1.5 (PointSpan, 2D → lifted to 3D)
// ============================================================================

TEST_CASE("face_normal_pointspan_right_2d",
          "[simplex][face_normal][pointspan][2d]") {
    // 2D right triangle: (0,0), (1,0), (0,1). Lifted to z=0.
    // Result should be (0, 0, 1) — same as 3D case.
    auto n = face_normal(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_normal_pointspan_equilateral_2d",
          "[simplex][face_normal][pointspan][2d]") {
    auto n = face_normal(sp(eq_a), sp(eq_b), sp(eq_c));
    // Normal in z-direction only, magnitude = 2 * area = sqrt(3)/2.
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n.norm(), WithinAbs(sqrt3 / 2.0, 1e-14));
}

TEST_CASE("face_normal_pointspan_2d_orientation",
          "[simplex][face_normal][pointspan][2d]") {
    // Reversed winding in 2D should flip the z-component.
    auto n_pos = face_normal(sp(rt_a), sp(rt_b), sp(rt_c));
    auto n_neg = face_normal(sp(rt_a), sp(rt_c), sp(rt_b));
    CHECK_THAT(n_pos(2), WithinAbs(-n_neg(2), 1e-14));
}

TEST_CASE("face_normal_pointspan_2d_3d_agreement",
          "[simplex][face_normal][pointspan]") {
    // 2D and 3D (z=0) should give the same result.
    std::array<double, 3> a3{0.0, 0.0, 0.0};
    std::array<double, 3> b3{1.0, 0.0, 0.0};
    std::array<double, 3> c3{0.0, 1.0, 0.0};
    auto n2d = face_normal(sp(rt_a), sp(rt_b), sp(rt_c));
    auto n3d = face_normal(sp(a3), sp(b3), sp(c3));
    for (std::size_t d = 0; d < 3; ++d) {
        CHECK_THAT(n2d(d), WithinAbs(n3d(d), 1e-14));
    }
}

// ============================================================================
// Face Normal — Degenerate triangle
// ============================================================================

TEST_CASE("face_normal_degenerate_triangle",
          "[simplex][face_normal][degenerate]") {
    // Collinear points: normal should be zero.
    auto n = face_normal(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(n.norm(), WithinAbs(0.0, 1e-14));
}

TEST_CASE("face_normal_degenerate_3d",
          "[simplex][face_normal][degenerate][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{2.0, 0.0, 0.0};
    auto n = face_normal(sp(a), sp(b), sp(c));
    CHECK_THAT(n.norm(), WithinAbs(0.0, 1e-14));
}

// ============================================================================
// Face Unit Normal
// ============================================================================

TEST_CASE("face_unit_normal_right_3d",
          "[simplex][face_unit_normal][triangle][3d]") {
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.0, 1.0, 0.0};
    auto n = face_unit_normal(sp(a), sp(b), sp(c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(1.0, 1e-14));
    // Unit length.
    CHECK_THAT(n.norm(), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_unit_normal_right_2d",
          "[simplex][face_unit_normal][triangle][2d]") {
    auto n = face_unit_normal(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(1.0, 1e-14));
    CHECK_THAT(n.norm(), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_unit_normal_tilted",
          "[simplex][face_unit_normal][triangle][3d]") {
    // Triangle: (0,0,0), (1,0,0), (0,0,1). Normal = (0,-1,0).
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    std::array<double, 3> c{0.0, 0.0, 1.0};
    auto n = face_unit_normal(sp(a), sp(b), sp(c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(-1.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n.norm(), WithinAbs(1.0, 1e-14));
}

TEST_CASE("face_unit_normal_degenerate_returns_zero",
          "[simplex][face_unit_normal][degenerate]") {
    // Degenerate (collinear) triangle: unit normal should be zero vector.
    auto n = face_unit_normal(sp(dg_a), sp(dg_b), sp(dg_c));
    CHECK_THAT(n(0), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(1), WithinAbs(0.0, 1e-14));
    CHECK_THAT(n(2), WithinAbs(0.0, 1e-14));
}

TEST_CASE("face_unit_normal_scaled_triangle",
          "[simplex][face_unit_normal][scale]") {
    // Scaling the triangle should not change the unit normal direction.
    std::array<double, 3> a1{0.0, 0.0, 0.0};
    std::array<double, 3> b1{1.0, 0.0, 0.0};
    std::array<double, 3> c1{0.0, 1.0, 0.0};

    std::array<double, 3> a2{0.0, 0.0, 0.0};
    std::array<double, 3> b2{100.0, 0.0, 0.0};
    std::array<double, 3> c2{0.0, 100.0, 0.0};

    auto n1 = face_unit_normal(sp(a1), sp(b1), sp(c1));
    auto n2 = face_unit_normal(sp(a2), sp(b2), sp(c2));
    for (std::size_t d = 0; d < 3; ++d) {
        CHECK_THAT(n1(d), WithinAbs(n2(d), 1e-14));
    }
}

// ============================================================================
// Face Normal — Consistency with volume
// ============================================================================

TEST_CASE("face_normal_magnitude_consistent_with_volume",
          "[simplex][face_normal][volume]") {
    // |face_normal| = 2 * area = 2 * volume_unsigned for a triangle.
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{3.0, 0.0, 0.0};
    std::array<double, 3> c{0.0, 4.0, 0.0};
    auto n = face_normal(sp(a), sp(b), sp(c));
    auto area = volume_unsigned(sp(a), sp(b), sp(c));
    CHECK_THAT(n.norm(), WithinAbs(2.0 * area, 1e-14));
}

TEST_CASE("face_normal_magnitude_consistent_with_volume_2d",
          "[simplex][face_normal][volume][2d]") {
    // Same check in 2D.
    auto n = face_normal(sp(rt_a), sp(rt_b), sp(rt_c));
    auto area = volume_unsigned(sp(rt_a), sp(rt_b), sp(rt_c));
    CHECK_THAT(n.norm(), WithinAbs(2.0 * area, 1e-14));
}

// ============================================================================
// Closest Point on Edge — 2D
// ============================================================================

TEST_CASE("closest_point_edge_2d_interior",
          "[simplex][closest_point][edge][2d]") {
    // Point above the midpoint of a horizontal edge.
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({0.5, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_edge_2d_at_vertex_a",
          "[simplex][closest_point][edge][2d]") {
    // Point closest to vertex a (beyond the a end).
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({-1.0, 0.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[0], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_edge_2d_at_vertex_b",
          "[simplex][closest_point][edge][2d]") {
    // Point closest to vertex b (beyond the b end).
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({2.0, 0.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_edge_2d_on_edge",
          "[simplex][closest_point][edge][2d]") {
    // Point on the edge itself — distance should be 0.
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({0.3, 0.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.distance_sq, WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[0] + result.barycentric[1],
               WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_edge_2d_degenerate",
          "[simplex][closest_point][edge][2d]") {
    // Degenerate edge (both endpoints identical).
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({1.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({0.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(2.0, 1e-14));
}

// ============================================================================
// Closest Point on Edge — 3D
// ============================================================================

TEST_CASE("closest_point_edge_3d_interior",
          "[simplex][closest_point][edge][3d]") {
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({0.5, 1.0, 0.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[2], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

// ============================================================================
// Closest Point on Triangle — 3D
// ============================================================================

TEST_CASE("closest_point_triangle_interior",
          "[simplex][closest_point][triangle][3d]") {
    // Point directly above the interior of a triangle in the xy-plane.
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({0.25, 0.25, 1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.25, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.25, 1e-14));
    CHECK_THAT(result.point[2], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
    // Barycentric should sum to 1.
    double bary_sum =
        result.barycentric[0] + result.barycentric[1] + result.barycentric[2];
    CHECK_THAT(bary_sum, WithinAbs(1.0, 1e-14));
    // All barycentric coords should be positive (interior).
    CHECK(result.barycentric[0] > 0.0);
    CHECK(result.barycentric[1] > 0.0);
    CHECK(result.barycentric[2] > 0.0);
}

TEST_CASE("closest_point_triangle_vertex_a",
          "[simplex][closest_point][triangle][3d]") {
    // Point closest to vertex A (in the negative quadrant).
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({-1.0, -1.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[0], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[2], WithinAbs(0.0, 1e-14));
}

TEST_CASE("closest_point_triangle_vertex_b",
          "[simplex][closest_point][triangle][3d]") {
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({2.0, -1.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_triangle_vertex_c",
          "[simplex][closest_point][triangle][3d]") {
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({-1.0, 2.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(1.0, 1e-14));
    CHECK_THAT(result.barycentric[2], WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_triangle_edge_ab",
          "[simplex][closest_point][triangle][3d]") {
    // Point closest to edge AB (below the triangle, in the x-axis region).
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({0.5, -1.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.barycentric[2], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_triangle_edge_ac",
          "[simplex][closest_point][triangle][3d]") {
    // Point closest to edge AC.
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({-1.0, 0.5, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.point[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.barycentric[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_triangle_edge_bc",
          "[simplex][closest_point][triangle][3d]") {
    // Point closest to edge BC (outside the hypotenuse).
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({1.0, 1.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    // Closest point on the line b-c is the midpoint (0.5, 0.5).
    CHECK_THAT(result.point[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.barycentric[0], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(0.5, 1e-14));
}

TEST_CASE("closest_point_triangle_on_surface",
          "[simplex][closest_point][triangle][3d]") {
    // Point on the triangle surface — distance should be 0.
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({0.2, 0.3, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto result = closest_point_on_simplex(p, verts);
    CHECK_THAT(result.distance_sq, WithinAbs(0.0, 1e-14));
}

// ============================================================================
// Closest Point — PointSpan layer
// ============================================================================

TEST_CASE("closest_point_edge_pointspan",
          "[simplex][closest_point][edge][pointspan]") {
    std::array<double, 2> p_arr{0.5, 1.0};
    std::array<std::array<double, 2>, 2> verts = {{{0.0, 0.0}, {1.0, 0.0}}};
    auto result = closest_point_on_simplex(sp(p_arr), verts);
    CHECK_THAT(result.point[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.0, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

TEST_CASE("closest_point_triangle_pointspan",
          "[simplex][closest_point][triangle][pointspan]") {
    std::array<double, 3> p_arr{0.25, 0.25, 1.0};
    std::array<std::array<double, 3>, 3> verts = {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}};
    auto result = closest_point_on_simplex(sp(p_arr), verts);
    CHECK_THAT(result.point[0], WithinAbs(0.25, 1e-14));
    CHECK_THAT(result.point[1], WithinAbs(0.25, 1e-14));
    CHECK_THAT(result.distance_sq, WithinAbs(1.0, 1e-14));
}

// ============================================================================
// distance_sq_to_simplex
// ============================================================================

TEST_CASE("distance_sq_to_edge", "[simplex][closest_point][distance]") {
    using Vec = ::zipper::Vector<double, 2>;
    Vec p({0.5, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    CHECK_THAT(distance_sq_to_simplex(p, verts), WithinAbs(1.0, 1e-14));
}

TEST_CASE("distance_sq_to_triangle", "[simplex][closest_point][distance]") {
    using Vec = ::zipper::Vector<double, 3>;
    Vec p({0.25, 0.25, 2.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    CHECK_THAT(distance_sq_to_simplex(p, verts), WithinAbs(4.0, 1e-14));
}

// ============================================================================
// Ray-Simplex Intersection — 2D (ray vs edge)
// ============================================================================

TEST_CASE("ray_edge_2d_hit", "[simplex][ray][edge][2d]") {
    // Ray from (0.5, -1) in +y direction hitting edge from (0,0) to (1,0).
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({0.5, -1.0});
    Vec dir({0.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
    CHECK_THAT(hit->barycentric[0], WithinAbs(0.5, 1e-14));
    CHECK_THAT(hit->barycentric[1], WithinAbs(0.5, 1e-14));
}

TEST_CASE("ray_edge_2d_miss", "[simplex][ray][edge][2d]") {
    // Ray parallel to edge — should miss.
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({0.0, 1.0});
    Vec dir({1.0, 0.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("ray_edge_2d_miss_outside_segment", "[simplex][ray][edge][2d]") {
    // Ray hitting the line but outside the edge segment.
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({2.0, -1.0});
    Vec dir({0.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("ray_edge_2d_at_vertex", "[simplex][ray][edge][2d]") {
    // Ray hitting exactly at vertex a.
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({0.0, -1.0});
    Vec dir({0.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
    CHECK_THAT(hit->barycentric[0], WithinAbs(1.0, 1e-14));
    CHECK_THAT(hit->barycentric[1], WithinAbs(0.0, 1e-14));
}

TEST_CASE("ray_edge_2d_negative_t", "[simplex][ray][edge][2d]") {
    // Ray origin above edge, pointing up — hit is behind (negative t).
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({0.5, 1.0});
    Vec dir({0.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK(hit->t < 0.0);
}

TEST_CASE("ray_edge_2d_backface_cull", "[simplex][ray][edge][2d]") {
    // With backface culling, one direction hits, the other is rejected.
    using Vec = ::zipper::Vector<double, 2>;
    Vec origin({0.5, -1.0});
    Vec dir({0.0, 1.0});
    std::array<Vec, 2> verts = {Vec({0.0, 0.0}), Vec({1.0, 0.0})};

    auto hit_front = ray_simplex_intersection(origin, dir, verts, true);
    // Reverse the edge orientation to test backface.
    std::array<Vec, 2> verts_rev = {Vec({1.0, 0.0}), Vec({0.0, 0.0})};
    auto hit_back = ray_simplex_intersection(origin, dir, verts_rev, true);

    // Exactly one of these should be culled.
    CHECK(hit_front.has_value() != hit_back.has_value());
}

// ============================================================================
// Ray-Simplex Intersection — 3D (ray vs triangle)
// ============================================================================

TEST_CASE("ray_triangle_3d_hit_center", "[simplex][ray][triangle][3d]") {
    // Ray from above, hitting the center of a triangle in the xy-plane.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.25, 0.25, 1.0});
    Vec dir({0.0, 0.0, -1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
    double bary_sum =
        hit->barycentric[0] + hit->barycentric[1] + hit->barycentric[2];
    CHECK_THAT(bary_sum, WithinAbs(1.0, 1e-14));
    CHECK(hit->barycentric[0] > 0.0);
    CHECK(hit->barycentric[1] > 0.0);
    CHECK(hit->barycentric[2] > 0.0);
}

TEST_CASE("ray_triangle_3d_miss_outside", "[simplex][ray][triangle][3d]") {
    // Ray aimed outside the triangle.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({2.0, 2.0, 1.0});
    Vec dir({0.0, 0.0, -1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("ray_triangle_3d_miss_parallel", "[simplex][ray][triangle][3d]") {
    // Ray parallel to the triangle plane.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.0, 0.0, 1.0});
    Vec dir({1.0, 0.0, 0.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("ray_triangle_3d_backface_cull", "[simplex][ray][triangle][3d]") {
    // With backface culling, hitting from the back should miss.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.25, 0.25, -1.0});
    Vec dir({0.0, 0.0, 1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};

    // Without backface culling: should hit.
    auto hit_no_cull = ray_simplex_intersection(origin, dir, verts, false);
    REQUIRE(hit_no_cull.has_value());

    // With backface culling: should miss (ray hits from behind).
    auto hit_cull = ray_simplex_intersection(origin, dir, verts, true);
    CHECK_FALSE(hit_cull.has_value());
}

TEST_CASE("ray_triangle_3d_hit_at_vertex", "[simplex][ray][triangle][3d]") {
    // Ray hitting exactly at vertex v0.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.0, 0.0, 1.0});
    Vec dir({0.0, 0.0, -1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
    CHECK_THAT(hit->barycentric[0], WithinAbs(1.0, 1e-14));
}

TEST_CASE("ray_triangle_3d_hit_on_edge", "[simplex][ray][triangle][3d]") {
    // Ray hitting the midpoint of edge v0-v1.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.5, 0.0, 1.0});
    Vec dir({0.0, 0.0, -1.0});
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->barycentric[2], WithinAbs(0.0, 1e-14));
}

TEST_CASE("ray_triangle_3d_oblique", "[simplex][ray][triangle][3d]") {
    // Oblique ray (not axis-aligned).
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.25, 0.25, 2.0});
    Vec dir({0.0, 0.0, -2.0}); // Non-unit direction.
    std::array<Vec, 3> verts = {
        Vec({0.0, 0.0, 0.0}), Vec({1.0, 0.0, 0.0}), Vec({0.0, 1.0, 0.0})};
    auto hit = ray_simplex_intersection(origin, dir, verts);
    REQUIRE(hit.has_value());
    // t * dir_z = -2, so origin.z + t*dir_z = 0 => t = 1.
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
}

// ============================================================================
// Ray-Simplex Intersection — PointSpan layer
// ============================================================================

TEST_CASE("ray_edge_2d_pointspan", "[simplex][ray][edge][pointspan]") {
    std::array<double, 2> origin{0.5, -1.0};
    std::array<double, 2> dir{0.0, 1.0};
    std::array<std::array<double, 2>, 2> verts = {{{0.0, 0.0}, {1.0, 0.0}}};
    auto hit = ray_simplex_intersection(sp(origin), sp(dir), verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
}

TEST_CASE("ray_triangle_3d_pointspan", "[simplex][ray][triangle][pointspan]") {
    std::array<double, 3> origin{0.25, 0.25, 1.0};
    std::array<double, 3> dir{0.0, 0.0, -1.0};
    std::array<std::array<double, 3>, 3> verts = {
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}};
    auto hit = ray_simplex_intersection(sp(origin), sp(dir), verts);
    REQUIRE(hit.has_value());
    CHECK_THAT(hit->t, WithinAbs(1.0, 1e-14));
}

// ============================================================================
// Ray-Edge Closest Approach — 3D (overdetermined case)
// ============================================================================

TEST_CASE("ray_edge_closest_approach_3d_intersecting",
          "[simplex][ray][edge][closest_approach]") {
    // Ray and edge that actually intersect.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.5, 0.0, -1.0});
    Vec dir({0.0, 0.0, 1.0});
    Vec a({0.0, 0.0, 0.0});
    Vec b({1.0, 0.0, 0.0});
    auto result = ray_edge_closest_approach(origin, dir, a, b, 1e-10);
    REQUIRE(result.has_value());
    CHECK_THAT(result->t_ray, WithinAbs(1.0, 1e-10));
    CHECK_THAT(result->t_edge, WithinAbs(0.5, 1e-10));
    CHECK_THAT(result->distance_sq, WithinAbs(0.0, 1e-20));
}

TEST_CASE("ray_edge_closest_approach_3d_skew",
          "[simplex][ray][edge][closest_approach]") {
    // Skew lines: ray along x at z=1, edge along y at z=0.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({-10.0, 0.5, 1.0});
    Vec dir({1.0, 0.0, 0.0});
    Vec a({0.0, 0.0, 0.0});
    Vec b({0.0, 1.0, 0.0});
    // Closest approach distance is 1.0 (the z-difference).
    auto result = ray_edge_closest_approach(origin, dir, a, b, 1.5);
    REQUIRE(result.has_value());
    CHECK_THAT(result->distance_sq, WithinAbs(1.0, 1e-10));
    CHECK_THAT(result->t_edge, WithinAbs(0.5, 1e-10));
}

TEST_CASE("ray_edge_closest_approach_3d_beyond_tolerance",
          "[simplex][ray][edge][closest_approach]") {
    // Same skew setup but with tight tolerance — should return nullopt.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({-10.0, 0.5, 1.0});
    Vec dir({1.0, 0.0, 0.0});
    Vec a({0.0, 0.0, 0.0});
    Vec b({0.0, 1.0, 0.0});
    auto result = ray_edge_closest_approach(origin, dir, a, b, 0.5);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("ray_edge_closest_approach_3d_parallel",
          "[simplex][ray][edge][closest_approach]") {
    // Ray and edge are parallel.
    using Vec = ::zipper::Vector<double, 3>;
    Vec origin({0.0, 0.0, 1.0});
    Vec dir({1.0, 0.0, 0.0});
    Vec a({0.0, 0.0, 0.0});
    Vec b({1.0, 0.0, 0.0});
    // Distance is 1.0 (z-difference), closest on edge is at t_edge=0 (nearest
    // to origin).
    auto result = ray_edge_closest_approach(origin, dir, a, b, 1.5);
    REQUIRE(result.has_value());
    CHECK_THAT(result->distance_sq, WithinAbs(1.0, 1e-10));
}

// ============================================================================
// Ray-Edge Closest Approach — PointSpan layer
// ============================================================================

TEST_CASE("ray_edge_closest_approach_pointspan",
          "[simplex][ray][edge][closest_approach][pointspan]") {
    std::array<double, 3> origin{0.5, 0.0, -1.0};
    std::array<double, 3> dir{0.0, 0.0, 1.0};
    std::array<double, 3> a{0.0, 0.0, 0.0};
    std::array<double, 3> b{1.0, 0.0, 0.0};
    auto result =
        ray_edge_closest_approach(sp(origin), sp(dir), sp(a), sp(b), 1e-10);
    REQUIRE(result.has_value());
    CHECK_THAT(result->distance_sq, WithinAbs(0.0, 1e-20));
}
