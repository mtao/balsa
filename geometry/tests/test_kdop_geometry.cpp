/// @file tests/spatial/test_kdop_geometry.cpp
/// Catch2 tests for kdop_geometry() — H-rep to V-rep conversion for k-DOPs.

#include <catch2/catch_all.hpp>

#include <balsa/geometry/spatial/KDOPGeometry.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

using namespace balsa::geometry::spatial;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;

// Use static_cast<size_t>(SD) in the array parameter to create a non-deduced
// context — SD is deduced solely from KDOPGeometry<double, SD>.

/// Check that a vertex with the given approximate coordinates exists.
template <int8_t SD>
auto has_vertex(const KDOPGeometry<double, SD> &geom,
                const std::array<double, static_cast<size_t>(SD)> &expected,
                double tol = 1e-8) -> bool {
    for (const auto &v : geom.vertices) {
        double d2 = 0.0;
        for (int8_t i = 0; i < SD; ++i) {
            double diff = v[i] - expected[i];
            d2 += diff * diff;
        }
        if (d2 < tol * tol) return true;
    }
    return false;
}

/// Check that an edge between two approximate vertex positions exists.
template <int8_t SD>
auto has_edge_between(const KDOPGeometry<double, SD> &geom,
                      const std::array<double, static_cast<size_t>(SD)> &a,
                      const std::array<double, static_cast<size_t>(SD)> &b,
                      double tol = 1e-8) -> bool {
    // Find vertex indices.
    auto find_idx =
        [&](const std::array<double, static_cast<size_t>(SD)> &target) -> int {
        for (size_t i = 0; i < geom.vertices.size(); ++i) {
            double d2 = 0.0;
            for (int8_t k = 0; k < SD; ++k) {
                double diff = geom.vertices[i][k] - target[k];
                d2 += diff * diff;
            }
            if (d2 < tol * tol) return static_cast<int>(i);
        }
        return -1;
    };

    int ia = find_idx(a);
    int ib = find_idx(b);
    if (ia < 0 || ib < 0) return false;

    auto ai = static_cast<size_t>(ia);
    auto bi = static_cast<size_t>(ib);
    for (const auto &[ei, ej] : geom.edges) {
        if ((ei == ai && ej == bi) || (ei == bi && ej == ai)) return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Empty / degenerate cases
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_empty", "[spatial][kdop_geometry]") {
    auto box = AABB<double, 3>::empty();
    auto geom = kdop_geometry(box);

    CHECK(geom.vertices.empty());
    CHECK(geom.edges.empty());
}

TEST_CASE("kdop_geometry_point_2d", "[spatial][kdop_geometry][2d]") {
    Vec2 p = {3.0, 5.0};
    auto box = AABB<double, 2>::from_point(p);
    auto geom = kdop_geometry(box);

    // A point KDOP has 1 vertex and 0 edges.
    CHECK(geom.vertices.size() == 1);
    CHECK(has_vertex(geom, p));
    CHECK(geom.edges.empty());
}

TEST_CASE("kdop_geometry_point_3d", "[spatial][kdop_geometry][3d]") {
    Vec3 p = {1.0, 2.0, 3.0};
    auto box = AABB<double, 3>::from_point(p);
    auto geom = kdop_geometry(box);

    CHECK(geom.vertices.size() == 1);
    CHECK(has_vertex(geom, p));
    CHECK(geom.edges.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// 2D AABB (K=2)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_2d_aabb_unit_square", "[spatial][kdop_geometry][2d]") {
    std::vector<Vec2> pts = {{0.0, 0.0}, {1.0, 1.0}};
    auto box = AABB<double, 2>::fit(pts);
    auto geom = kdop_geometry(box);

    REQUIRE(geom.vertices.size() == 4);
    REQUIRE(geom.edges.size() == 4);

    // All four corners present.
    CHECK(has_vertex(geom, {0.0, 0.0}));
    CHECK(has_vertex(geom, {1.0, 0.0}));
    CHECK(has_vertex(geom, {0.0, 1.0}));
    CHECK(has_vertex(geom, {1.0, 1.0}));

    // Four edges forming the boundary.
    CHECK(has_edge_between(geom, {0.0, 0.0}, {1.0, 0.0}));
    CHECK(has_edge_between(geom, {1.0, 0.0}, {1.0, 1.0}));
    CHECK(has_edge_between(geom, {1.0, 1.0}, {0.0, 1.0}));
    CHECK(has_edge_between(geom, {0.0, 1.0}, {0.0, 0.0}));

    // No diagonal edges.
    CHECK_FALSE(has_edge_between(geom, {0.0, 0.0}, {1.0, 1.0}));
    CHECK_FALSE(has_edge_between(geom, {1.0, 0.0}, {0.0, 1.0}));
}

TEST_CASE("kdop_geometry_2d_aabb_rectangle", "[spatial][kdop_geometry][2d]") {
    std::vector<Vec2> pts = {{-1.0, 2.0}, {3.0, 5.0}};
    auto box = AABB<double, 2>::fit(pts);
    auto geom = kdop_geometry(box);

    REQUIRE(geom.vertices.size() == 4);
    REQUIRE(geom.edges.size() == 4);

    CHECK(has_vertex(geom, {-1.0, 2.0}));
    CHECK(has_vertex(geom, {3.0, 2.0}));
    CHECK(has_vertex(geom, {-1.0, 5.0}));
    CHECK(has_vertex(geom, {3.0, 5.0}));
}

// ═══════════════════════════════════════════════════════════════════════════
// 3D AABB (K=3)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_3d_aabb_unit_cube", "[spatial][kdop_geometry][3d]") {
    std::vector<Vec3> pts = {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    auto box = AABB<double, 3>::fit(pts);
    auto geom = kdop_geometry(box);

    REQUIRE(geom.vertices.size() == 8);
    REQUIRE(geom.edges.size() == 12);

    // All eight corners.
    for (double x : {0.0, 1.0})
        for (double y : {0.0, 1.0})
            for (double z : {0.0, 1.0}) CHECK(has_vertex(geom, {x, y, z}));

    // 12 edges — each pair of corners differing in exactly one coordinate.
    for (double x : {0.0, 1.0})
        for (double y : {0.0, 1.0})
            for (double z : {0.0, 1.0}) {
                if (x == 0.0)
                    CHECK(has_edge_between(geom, {0.0, y, z}, {1.0, y, z}));
                if (y == 0.0)
                    CHECK(has_edge_between(geom, {x, 0.0, z}, {x, 1.0, z}));
                if (z == 0.0)
                    CHECK(has_edge_between(geom, {x, y, 0.0}, {x, y, 1.0}));
            }

    // No face diagonals.
    CHECK_FALSE(has_edge_between(geom, {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}));
    // No space diagonals.
    CHECK_FALSE(has_edge_between(geom, {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}));
}

// ═══════════════════════════════════════════════════════════════════════════
// 2D 4-DOP (K=4)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_2d_4dop_unit_square", "[spatial][kdop_geometry][2d]") {
    // Fit a 4-DOP around the unit square.  The extra diagonal slabs are
    // tight to the square, so the 4-DOP equals the square (the diagonal
    // constraints don't clip any corners).
    using DOP4 = KDOP<double, 2, 4>;
    std::vector<Vec2> pts = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};
    auto dop = DOP4::fit(pts);
    auto geom = kdop_geometry(dop);

    // A unit square under 4-DOP directions (axes + diagonals) is the square
    // itself: 4 vertices, 4 edges.
    REQUIRE(geom.vertices.size() == 4);
    REQUIRE(geom.edges.size() == 4);

    CHECK(has_vertex(geom, {0.0, 0.0}));
    CHECK(has_vertex(geom, {1.0, 0.0}));
    CHECK(has_vertex(geom, {0.0, 1.0}));
    CHECK(has_vertex(geom, {1.0, 1.0}));
}

TEST_CASE("kdop_geometry_2d_4dop_clipped_octagon",
          "[spatial][kdop_geometry][2d]") {
    // Fit a 4-DOP around a circle approximation.  Points on the unit circle
    // will produce slab bounds that clip the AABB corners, giving an octagon.
    using DOP4 = KDOP<double, 2, 4>;

    // 8 points evenly spaced on the unit circle.
    std::vector<Vec2> pts;
    for (int i = 0; i < 8; ++i) {
        double theta = i * 3.14159265358979323846 / 4.0;
        pts.push_back({std::cos(theta), std::sin(theta)});
    }
    auto dop = DOP4::fit(pts);
    auto geom = kdop_geometry(dop);

    // An octagon: 8 vertices, 8 edges.
    CHECK(geom.vertices.size() == 8);
    CHECK(geom.edges.size() == 8);
}

// ═══════════════════════════════════════════════════════════════════════════
// 3D 13-DOP (K=13)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_3d_13dop_unit_cube", "[spatial][kdop_geometry][3d]") {
    // 13-DOP around a unit cube.  The extra diagonal slabs are tight to the
    // cube, so the 13-DOP equals the cube: 8 vertices, 12 edges.
    using DOP13 = KDOP<double, 3, 13>;
    std::vector<Vec3> cube_verts;
    for (double x : {0.0, 1.0})
        for (double y : {0.0, 1.0})
            for (double z : {0.0, 1.0}) cube_verts.push_back({x, y, z});

    auto dop = DOP13::fit(cube_verts);
    auto geom = kdop_geometry(dop);

    CHECK(geom.vertices.size() == 8);
    CHECK(geom.edges.size() == 12);

    for (double x : {0.0, 1.0})
        for (double y : {0.0, 1.0})
            for (double z : {0.0, 1.0}) CHECK(has_vertex(geom, {x, y, z}));
}

TEST_CASE("kdop_geometry_3d_13dop_sphere_approx",
          "[spatial][kdop_geometry][3d]") {
    // 13-DOP around points on the unit sphere.  The extra diagonal slabs
    // should clip the AABB corners, giving more than 8 vertices.
    using DOP13 = KDOP<double, 3, 13>;

    // 26 points on the unit sphere (along each of the 13 directions,
    // plus their negations).
    constexpr auto dirs = DOP13::directions();
    std::vector<Vec3> pts;
    for (const auto &d : dirs) {
        double norm = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        pts.push_back({d[0] / norm, d[1] / norm, d[2] / norm});
        pts.push_back({-d[0] / norm, -d[1] / norm, -d[2] / norm});
    }

    auto dop = DOP13::fit(pts);
    auto geom = kdop_geometry(dop);

    // Should have a polyhedron with more vertices/edges than a cube.
    CHECK(geom.vertices.size() > 8);
    CHECK(geom.edges.size() > 12);

    // Every vertex should lie inside (or on boundary of) the k-DOP.
    for (const auto &v : geom.vertices) { CHECK(dop.contains(v)); }

    // Every edge endpoint pair should both be valid vertices.
    for (const auto &[ei, ej] : geom.edges) {
        CHECK(ei < geom.vertices.size());
        CHECK(ej < geom.vertices.size());
        CHECK(ei != ej);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 3D 9-DOP (K=9)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_3d_9dop_unit_cube", "[spatial][kdop_geometry][3d]") {
    using DOP9 = KDOP<double, 3, 9>;
    std::vector<Vec3> cube_verts;
    for (double x : {0.0, 1.0})
        for (double y : {0.0, 1.0})
            for (double z : {0.0, 1.0}) cube_verts.push_back({x, y, z});

    auto dop = DOP9::fit(cube_verts);
    auto geom = kdop_geometry(dop);

    // 9-DOP tight to a cube is still the cube.
    CHECK(geom.vertices.size() == 8);
    CHECK(geom.edges.size() == 12);
}

// ═══════════════════════════════════════════════════════════════════════════
// Structural invariants
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_geometry_edges_are_sorted_pairs", "[spatial][kdop_geometry]") {
    std::vector<Vec3> pts = {{0.0, 0.0, 0.0}, {2.0, 3.0, 5.0}};
    auto box = AABB<double, 3>::fit(pts);
    auto geom = kdop_geometry(box);

    for (const auto &[ei, ej] : geom.edges) { CHECK(ei < ej); }
}

TEST_CASE("kdop_geometry_no_duplicate_edges", "[spatial][kdop_geometry]") {
    using DOP13 = KDOP<double, 3, 13>;
    std::vector<Vec3> pts = {{-1.0, -1.0, -1.0}, {1.0, 1.0, 1.0}};
    auto dop = DOP13::fit(pts);
    auto geom = kdop_geometry(dop);

    std::set<std::pair<size_t, size_t>> edge_set(geom.edges.begin(),
                                                 geom.edges.end());
    CHECK(edge_set.size() == geom.edges.size());
}

TEST_CASE("kdop_geometry_euler_formula_3d", "[spatial][kdop_geometry][3d]") {
    // For a convex 3D polyhedron: V - E + F = 2.
    // We don't compute faces yet, but we can check V - E is reasonable.
    // For a cube: V=8, E=12, F=6 → V-E+F = 2, so V-E = -4.
    std::vector<Vec3> pts = {{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    auto box = AABB<double, 3>::fit(pts);
    auto geom = kdop_geometry(box);

    int V = static_cast<int>(geom.vertices.size());
    int E = static_cast<int>(geom.edges.size());
    // For a cube: V - E = 8 - 12 = -4, so F = 2 - (V - E) = 6.
    int F = 2 - V + E;
    CHECK(F == 6);
}
