/// @file tests/spatial/test_kdop.cpp
/// Catch2 tests for KDOP<SpatialDim, K, DirectionPolicy> and the AABB alias.

#include <catch2/catch_all.hpp>

#include <balsa/geometry/spatial/KDOP.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace balsa::geometry::spatial;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;

// ═══════════════════════════════════════════════════════════════════════════
// AABB<double, 2> (= KDOP<2, 2>) tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb2_from_point", "[spatial][aabb][2d]") {
    Vec2 p = {3.0, 5.0};
    auto box = AABB<double, 2>::from_point(p);

    CHECK(box.min(0) == 3.0);
    CHECK(box.max(0) == 3.0);
    CHECK(box.min(1) == 5.0);
    CHECK(box.max(1) == 5.0);
    CHECK(box.volume() == 0.0);
}

TEST_CASE("aabb2_expand", "[spatial][aabb][2d]") {
    Vec2 p0 = {1.0, 2.0};
    Vec2 p1 = {4.0, 6.0};
    auto box = AABB<double, 2>::from_point(p0);
    box.expand(p1);

    CHECK(box.min(0) == 1.0);
    CHECK(box.max(0) == 4.0);
    CHECK(box.min(1) == 2.0);
    CHECK(box.max(1) == 6.0);
    CHECK(box.width(0) == 3.0);
    CHECK(box.width(1) == 4.0);
    CHECK(box.volume() == Catch::Approx(12.0));
    CHECK(box.surface_area() == Catch::Approx(14.0));
}

TEST_CASE("aabb2_fit", "[spatial][aabb][2d]") {
    std::vector<Vec2> pts = {{0.0, 0.0}, {3.0, 1.0}, {1.0, 4.0}};
    auto box = AABB<double, 2>::fit(pts);

    CHECK(box.min(0) == 0.0);
    CHECK(box.max(0) == 3.0);
    CHECK(box.min(1) == 0.0);
    CHECK(box.max(1) == 4.0);
}

TEST_CASE("aabb2_center", "[spatial][aabb][2d]") {
    std::vector<Vec2> pts = {{0.0, 2.0}, {4.0, 8.0}};
    auto box = AABB<double, 2>::fit(pts);
    auto c = box.center();

    CHECK(c[0] == Catch::Approx(2.0));
    CHECK(c[1] == Catch::Approx(5.0));
}

TEST_CASE("aabb2_overlaps", "[spatial][aabb][2d]") {
    auto a = AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {2.0, 2.0}});
    auto b = AABB<double, 2>::fit(std::vector<Vec2>{{1.0, 1.0}, {3.0, 3.0}});
    auto c = AABB<double, 2>::fit(std::vector<Vec2>{{5.0, 5.0}, {6.0, 6.0}});

    CHECK(a.overlaps(b));
    CHECK(b.overlaps(a));
    CHECK_FALSE(a.overlaps(c));
    CHECK_FALSE(c.overlaps(a));
}

TEST_CASE("aabb2_touching_overlaps", "[spatial][aabb][2d]") {
    auto a = AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {1.0, 1.0}});
    auto b = AABB<double, 2>::fit(std::vector<Vec2>{{1.0, 0.0}, {2.0, 1.0}});

    // Touching edges count as overlapping.
    CHECK(a.overlaps(b));
}

TEST_CASE("aabb2_merge", "[spatial][aabb][2d]") {
    auto a = AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {1.0, 1.0}});
    auto b = AABB<double, 2>::fit(std::vector<Vec2>{{3.0, 4.0}, {5.0, 6.0}});
    auto m = a.merge(b);

    CHECK(m.min(0) == 0.0);
    CHECK(m.max(0) == 5.0);
    CHECK(m.min(1) == 0.0);
    CHECK(m.max(1) == 6.0);
}

TEST_CASE("aabb2_empty", "[spatial][aabb][2d]") {
    auto box = AABB<double, 2>::empty();

    // Empty box should not overlap anything.
    auto other =
        AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {1.0, 1.0}});
    CHECK_FALSE(box.overlaps(other));

    // Expanding empty box should give the point's box.
    Vec2 p = {2.0, 3.0};
    box.expand(p);
    CHECK(box.min(0) == 2.0);
    CHECK(box.max(0) == 2.0);
    CHECK(box.min(1) == 3.0);
    CHECK(box.max(1) == 3.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// AABB<double, 3> (= KDOP<3, 3>) tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb3_fit", "[spatial][aabb][3d]") {
    std::vector<Vec3> pts = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {0.0, 3.0, 1.0}};
    auto box = AABB<double, 3>::fit(pts);

    CHECK(box.min(0) == 0.0);
    CHECK(box.max(0) == 4.0);
    CHECK(box.min(1) == 2.0);
    CHECK(box.max(1) == 5.0);
    CHECK(box.min(2) == 1.0);
    CHECK(box.max(2) == 6.0);
    CHECK(box.volume() == Catch::Approx(4.0 * 3.0 * 5.0));
    CHECK(box.surface_area()
          == Catch::Approx(2.0 * (4.0 * 3.0 + 4.0 * 5.0 + 3.0 * 5.0)));
}

TEST_CASE("aabb3_overlaps_separated", "[spatial][aabb][3d]") {
    auto a = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    auto b = AABB<double, 3>::fit(std::vector<Vec3>{{2, 2, 2}, {3, 3, 3}});
    CHECK_FALSE(a.overlaps(b));
}

TEST_CASE("aabb3_overlaps_partial", "[spatial][aabb][3d]") {
    auto a = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {2, 2, 2}});
    auto b = AABB<double, 3>::fit(std::vector<Vec3>{{1, 1, 1}, {3, 3, 3}});
    CHECK(a.overlaps(b));
}

TEST_CASE("aabb3_center", "[spatial][aabb][3d]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {4, 6, 8}});
    auto c = box.center();
    CHECK(c[0] == Catch::Approx(2.0));
    CHECK(c[1] == Catch::Approx(3.0));
    CHECK(c[2] == Catch::Approx(4.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// KDOP<double, 2, 4> tests (2D with diagonals)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_2d_4_fit", "[spatial][kdop][2d]") {
    // A unit square: vertices at (0,0), (1,0), (1,1), (0,1)
    std::vector<Vec2> pts = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    auto kdop = KDOP<double, 2, 4>::fit(pts);

    // Axis-aligned slabs: [0,1] x [0,1]
    CHECK(kdop.min(0) == 0.0);
    CHECK(kdop.max(0) == 1.0);
    CHECK(kdop.min(1) == 0.0);
    CHECK(kdop.max(1) == 1.0);

    // Diagonal (1,1): projections are x+y -> [0, 2]
    CHECK(kdop.min(2) == Catch::Approx(0.0));
    CHECK(kdop.max(2) == Catch::Approx(2.0));

    // Diagonal (1,-1): projections are x-y -> [-1, 1]
    CHECK(kdop.min(3) == Catch::Approx(-1.0));
    CHECK(kdop.max(3) == Catch::Approx(1.0));
}

TEST_CASE("kdop_2d_4_tighter_than_aabb", "[spatial][kdop][2d]") {
    // A diamond shape rotated 45 degrees.
    // The 4-DOP should be tighter than the AABB for this shape.
    std::vector<Vec2> pts = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    auto aabb = AABB<double, 2>::fit(pts);
    auto kdop = KDOP<double, 2, 4>::fit(pts);

    // AABB: [-1,1] x [-1,1], area = 4
    CHECK(aabb.volume() == Catch::Approx(4.0));

    // 4-DOP: axis slabs are same [-1,1] x [-1,1], but diagonal slabs
    // are tighter: (1,1) -> [-1,1], (1,-1) -> [-1,1]
    // The AABB volume is an approximation for the 4-DOP volume too,
    // but the 4-DOP overlap test is strictly tighter.
    // Verify that a point outside the diamond but inside the AABB
    // can be distinguished by the 4-DOP.
    Vec2 corner = {0.9, 0.9};
    auto corner_aabb = AABB<double, 2>::from_point(corner);
    auto corner_kdop = KDOP<double, 2, 4>::from_point(corner);

    CHECK(aabb.overlaps(corner_aabb));
    // The 4-DOP should NOT overlap because (1,1) dot (0.9, 0.9) = 1.8 > 1.0
    CHECK_FALSE(kdop.overlaps(corner_kdop));
}

// ═══════════════════════════════════════════════════════════════════════════
// KDOP<double, 3, 9> tests (3D with face diagonals)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_3d_9_fit", "[spatial][kdop][3d]") {
    std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    auto kdop = KDOP<double, 3, 9>::fit(pts);

    // Axis-aligned slabs
    CHECK(kdop.min(0) == 0.0);
    CHECK(kdop.max(0) == 1.0);
    CHECK(kdop.min(1) == 0.0);
    CHECK(kdop.max(1) == 1.0);
    CHECK(kdop.min(2) == 0.0);
    CHECK(kdop.max(2) == 1.0);

    // Face diagonal (1,1,0): projections are x+y -> [0, 1]
    CHECK(kdop.min(3) == Catch::Approx(0.0));
    CHECK(kdop.max(3) == Catch::Approx(1.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// KDOP<double, 3, 13> tests (3D with face + space diagonals)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_3d_13_fit", "[spatial][kdop][3d]") {
    // Unit cube vertices
    std::vector<Vec3> pts = {{0, 0, 0},
                             {1, 0, 0},
                             {0, 1, 0},
                             {0, 0, 1},
                             {1, 1, 0},
                             {1, 0, 1},
                             {0, 1, 1},
                             {1, 1, 1}};
    auto kdop = KDOP<double, 3, 13>::fit(pts);

    // Axis slabs: [0,1] each
    for (int8_t i = 0; i < 3; ++i) {
        CHECK(kdop.min(i) == 0.0);
        CHECK(kdop.max(i) == 1.0);
    }

    // Space diagonal (1,1,1): projections are x+y+z -> [0, 3]
    CHECK(kdop.min(9) == Catch::Approx(0.0));
    CHECK(kdop.max(9) == Catch::Approx(3.0));
}

TEST_CASE("kdop_3d_13_overlaps", "[spatial][kdop][3d]") {
    auto a = KDOP<double, 3, 13>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    auto b = KDOP<double, 3, 13>::fit(std::vector<Vec3>{{2, 2, 2}, {3, 3, 3}});
    CHECK_FALSE(a.overlaps(b));

    auto c = KDOP<double, 3, 13>::fit(
        std::vector<Vec3>{{0.5, 0.5, 0.5}, {1.5, 1.5, 1.5}});
    CHECK(a.overlaps(c));
}

// ═══════════════════════════════════════════════════════════════════════════
// Custom direction policy
// ═══════════════════════════════════════════════════════════════════════════

struct CustomDirections2D {
    static consteval auto directions() -> std::array<std::array<double, 2>, 3> {
        return {{
            {1.0, 0.0},
            {0.0, 1.0},
            {1.0, 2.0}, // Custom non-standard direction
        }};
    }
};

TEST_CASE("kdop_custom_policy", "[spatial][kdop][custom]") {
    using CustomKDOP = KDOP<double, 2, 3, CustomDirections2D>;

    std::vector<Vec2> pts = {{0, 0}, {1, 0}, {0, 1}};
    auto kdop = CustomKDOP::fit(pts);

    // Axis slabs
    CHECK(kdop.min(0) == 0.0);
    CHECK(kdop.max(0) == 1.0);
    CHECK(kdop.min(1) == 0.0);
    CHECK(kdop.max(1) == 1.0);

    // Custom direction (1,2): projections are x+2y -> [0, 2]
    CHECK(kdop.min(2) == Catch::Approx(0.0));
    CHECK(kdop.max(2) == Catch::Approx(2.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// Merge / merge_in_place
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_merge_in_place", "[spatial][kdop]") {
    auto a = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    auto b = AABB<double, 3>::fit(std::vector<Vec3>{{2, 3, 4}, {5, 6, 7}});

    auto m = a.merge(b);
    a.merge_in_place(b);

    CHECK(a.min(0) == m.min(0));
    CHECK(a.max(0) == m.max(0));
    CHECK(a.min(1) == m.min(1));
    CHECK(a.max(1) == m.max(1));
    CHECK(a.min(2) == m.min(2));
    CHECK(a.max(2) == m.max(2));

    CHECK(m.min(0) == 0.0);
    CHECK(m.max(0) == 5.0);
    CHECK(m.min(1) == 0.0);
    CHECK(m.max(1) == 6.0);
    CHECK(m.min(2) == 0.0);
    CHECK(m.max(2) == 7.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Slab vector access
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_slab_vector_access", "[spatial][kdop]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{1, 2, 3}, {4, 5, 6}});

    const auto &lo = box.slab_min();
    const auto &hi = box.slab_max();

    CHECK(lo(0) == 1.0);
    CHECK(lo(1) == 2.0);
    CHECK(lo(2) == 3.0);
    CHECK(hi(0) == 4.0);
    CHECK(hi(1) == 5.0);
    CHECK(hi(2) == 6.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Edge cases
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_single_point_volume_zero", "[spatial][kdop]") {
    auto box = AABB<double, 3>::from_point(Vec3{1.0, 2.0, 3.0});
    CHECK(box.volume() == 0.0);
}

TEST_CASE("kdop_flat_simplex", "[spatial][kdop]") {
    // Degenerate: all points on a line.
    std::vector<Vec3> pts = {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}};
    auto box = AABB<double, 3>::fit(pts);

    CHECK(box.min(0) == 0.0);
    CHECK(box.max(0) == 2.0);
    CHECK(box.width(1) == 0.0);
    CHECK(box.width(2) == 0.0);
    CHECK(box.volume() == 0.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Direction table verification
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("default_directions_2d_2", "[spatial][directions]") {
    auto dirs = DefaultDirections<2, 2>::directions();
    // First SpatialDim directions should be axis-aligned.
    CHECK(dirs[0] == Vec2{1.0, 0.0});
    CHECK(dirs[1] == Vec2{0.0, 1.0});
}

TEST_CASE("default_directions_3d_3", "[spatial][directions]") {
    auto dirs = DefaultDirections<3, 3>::directions();
    CHECK(dirs[0] == Vec3{1.0, 0.0, 0.0});
    CHECK(dirs[1] == Vec3{0.0, 1.0, 0.0});
    CHECK(dirs[2] == Vec3{0.0, 0.0, 1.0});
}

TEST_CASE("default_directions_first_axes", "[spatial][directions]") {
    // For all default policies, the first SpatialDim directions must be
    // the coordinate axes (so that AABB semantics apply to slabs 0..D-1).
    {
        auto dirs = DefaultDirections<2, 4>::directions();
        CHECK(dirs[0] == Vec2{1.0, 0.0});
        CHECK(dirs[1] == Vec2{0.0, 1.0});
    }
    {
        auto dirs = DefaultDirections<3, 9>::directions();
        CHECK(dirs[0] == Vec3{1.0, 0.0, 0.0});
        CHECK(dirs[1] == Vec3{0.0, 1.0, 0.0});
        CHECK(dirs[2] == Vec3{0.0, 0.0, 1.0});
    }
    {
        auto dirs = DefaultDirections<3, 13>::directions();
        CHECK(dirs[0] == Vec3{1.0, 0.0, 0.0});
        CHECK(dirs[1] == Vec3{0.0, 1.0, 0.0});
        CHECK(dirs[2] == Vec3{0.0, 0.0, 1.0});
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Equality
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_equality", "[spatial][kdop]") {
    auto a = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {1, 1}});
    auto b = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {1, 1}});
    auto c = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});

    CHECK(a == b);
    CHECK_FALSE(a == c);
}

// ═══════════════════════════════════════════════════════════════════════════
// is_empty()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_is_empty", "[spatial][kdop]") {
    auto empty_box = AABB<double, 3>::empty();
    CHECK(empty_box.is_empty());

    auto point_box = AABB<double, 3>::from_point(Vec3{1.0, 2.0, 3.0});
    CHECK_FALSE(point_box.is_empty());

    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {1, 1}});
    CHECK_FALSE(box.is_empty());
}

TEST_CASE("kdop_is_empty_after_expand", "[spatial][kdop]") {
    auto box = AABB<double, 2>::empty();
    CHECK(box.is_empty());

    box.expand(Vec2{1.0, 2.0});
    CHECK_FALSE(box.is_empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// contains()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb2_contains", "[spatial][aabb][2d]") {
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {2.0, 2.0}});

    // Interior point
    CHECK(box.contains(Vec2{1.0, 1.0}));

    // Boundary points
    CHECK(box.contains(Vec2{0.0, 0.0}));
    CHECK(box.contains(Vec2{2.0, 2.0}));
    CHECK(box.contains(Vec2{0.0, 1.0}));

    // Outside
    CHECK_FALSE(box.contains(Vec2{3.0, 1.0}));
    CHECK_FALSE(box.contains(Vec2{1.0, -0.1}));
    CHECK_FALSE(box.contains(Vec2{-0.1, -0.1}));
}

TEST_CASE("aabb3_contains", "[spatial][aabb][3d]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});

    CHECK(box.contains(Vec3{0.5, 0.5, 0.5}));
    CHECK(box.contains(Vec3{0.0, 0.0, 0.0}));
    CHECK(box.contains(Vec3{1.0, 1.0, 1.0}));
    CHECK_FALSE(box.contains(Vec3{1.1, 0.5, 0.5}));
    CHECK_FALSE(box.contains(Vec3{0.5, 0.5, -0.1}));
}

TEST_CASE("kdop_2d_4_contains", "[spatial][kdop][2d]") {
    // Diamond: vertices at (1,0), (0,1), (-1,0), (0,-1)
    std::vector<Vec2> pts = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    auto kdop = KDOP<double, 2, 4>::fit(pts);

    // Center
    CHECK(kdop.contains(Vec2{0.0, 0.0}));

    // On the boundary axis-aligned
    CHECK(kdop.contains(Vec2{1.0, 0.0}));

    // Inside the AABB but outside the 4-DOP (corner of bounding square)
    // (0.9, 0.9): proj on (1,1) = 1.8, but 4-DOP has (1,1) slab = [-1, 1]
    CHECK_FALSE(kdop.contains(Vec2{0.9, 0.9}));
}

TEST_CASE("kdop_empty_contains_nothing", "[spatial][kdop]") {
    auto box = AABB<double, 2>::empty();
    CHECK_FALSE(box.contains(Vec2{0.0, 0.0}));
}

// ═══════════════════════════════════════════════════════════════════════════
// inflate()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb2_inflate", "[spatial][aabb][2d]") {
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{1.0, 1.0}, {3.0, 3.0}});

    box.inflate(0.5);

    CHECK(box.min(0) == Catch::Approx(0.5));
    CHECK(box.max(0) == Catch::Approx(3.5));
    CHECK(box.min(1) == Catch::Approx(0.5));
    CHECK(box.max(1) == Catch::Approx(3.5));
    CHECK(box.width(0) == Catch::Approx(3.0));
    CHECK(box.width(1) == Catch::Approx(3.0));
}

TEST_CASE("aabb3_inflate", "[spatial][aabb][3d]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});

    box.inflate(0.1);

    CHECK(box.min(0) == Catch::Approx(-0.1));
    CHECK(box.max(0) == Catch::Approx(1.1));
    CHECK(box.min(1) == Catch::Approx(-0.1));
    CHECK(box.max(1) == Catch::Approx(1.1));
    CHECK(box.min(2) == Catch::Approx(-0.1));
    CHECK(box.max(2) == Catch::Approx(1.1));
}

TEST_CASE("kdop_inflate_affects_contains", "[spatial][kdop][2d]") {
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0.0, 0.0}, {1.0, 1.0}});

    // Point outside the original box
    Vec2 p = {1.2, 0.5};
    CHECK_FALSE(box.contains(p));

    // After inflating by 0.3, the point should be inside
    box.inflate(0.3);
    CHECK(box.contains(p));
}

TEST_CASE("kdop_inflate_affects_overlap", "[spatial][kdop][3d]") {
    auto a = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    auto b = AABB<double, 3>::fit(std::vector<Vec3>{{1.5, 0, 0}, {2.5, 1, 1}});

    CHECK_FALSE(a.overlaps(b));

    a.inflate(0.6);
    CHECK(a.overlaps(b));
}

// ═══════════════════════════════════════════════════════════════════════════
// diagonal() (AABB-only)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb2_diagonal", "[spatial][aabb][2d]") {
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {3, 4}});
    CHECK(box.diagonal() == Catch::Approx(5.0));
}

TEST_CASE("aabb3_diagonal", "[spatial][aabb][3d]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{1, 2, 3}, {4, 6, 3}});
    // widths: 3, 4, 0 → diagonal = sqrt(9 + 16 + 0) = 5
    CHECK(box.diagonal() == Catch::Approx(5.0));
}

TEST_CASE("aabb3_diagonal_unit_cube", "[spatial][aabb][3d]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    CHECK(box.diagonal() == Catch::Approx(std::sqrt(3.0)));
}

TEST_CASE("aabb_diagonal_single_point_is_zero", "[spatial][aabb]") {
    auto box = AABB<double, 3>::from_point(Vec3{5.0, 5.0, 5.0});
    CHECK(box.diagonal() == 0.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// ray_intersect — AABB<double, 2> tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb2_ray_hit_from_outside", "[spatial][aabb][2d][ray]") {
    // Unit square [0,2]×[0,2], ray from (-1, 1) in +x direction.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});
    Vec2 origin = {-1.0, 1.0};
    Vec2 dir = {1.0, 0.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0)); // Enters at x=0
    CHECK(hit->second == Catch::Approx(3.0)); // Exits at x=2
}

TEST_CASE("aabb2_ray_origin_inside", "[spatial][aabb][2d][ray]") {
    // Ray origin inside the box.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});
    Vec2 origin = {1.0, 1.0};
    Vec2 dir = {1.0, 0.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first < 0.0); // t_near is negative (behind origin)
    CHECK(hit->second > 0.0); // t_far is positive (ahead)
}

TEST_CASE("aabb2_ray_miss", "[spatial][aabb][2d][ray]") {
    // Ray parallel to box, outside.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});
    Vec2 origin = {-1.0, 3.0}; // Above the box
    Vec2 dir = {1.0, 0.0};
    auto hit = box.ray_intersect(origin, dir);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("aabb2_ray_miss_perpendicular", "[spatial][aabb][2d][ray]") {
    // Ray pointing away from the box.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});
    Vec2 origin = {-1.0, 1.0};
    Vec2 dir = {-1.0, 0.0}; // Pointing away
    auto hit = box.ray_intersect(origin, dir);
    // Should return valid interval but both t values are negative
    REQUIRE(hit.has_value());
    CHECK(hit->second < 0.0);
}

TEST_CASE("aabb2_ray_graze_corner", "[spatial][aabb][2d][ray]") {
    // Diagonal ray through unit square from (-1,-1) in direction (1,1).
    // The ray enters at corner (0,0) at t=1 and exits at corner (1,1) at t=2
    // (since direction is unnormalized).
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {1, 1}});
    Vec2 origin = {-1.0, -1.0};
    Vec2 dir = {1.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0));
    CHECK(hit->second == Catch::Approx(2.0));
}

TEST_CASE("aabb2_ray_diagonal", "[spatial][aabb][2d][ray]") {
    // Diagonal ray through unit square.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {1, 1}});
    Vec2 origin = {-0.5, -0.5};
    Vec2 dir = {1.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(0.5)); // Enters at (0,0)
    CHECK(hit->second == Catch::Approx(1.5)); // Exits at (1,1)
}

// ═══════════════════════════════════════════════════════════════════════════
// ray_intersect — AABB<double, 3> tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("aabb3_ray_hit_axis_aligned", "[spatial][aabb][3d][ray]") {
    // Unit cube [0,1]^3, ray along +z axis through center.
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    Vec3 origin = {0.5, 0.5, -1.0};
    Vec3 dir = {0.0, 0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0)); // Enters at z=0
    CHECK(hit->second == Catch::Approx(2.0)); // Exits at z=1
}

TEST_CASE("aabb3_ray_hit_diagonal", "[spatial][aabb][3d][ray]") {
    // Diagonal ray through unit cube.
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    Vec3 origin = {-1.0, -1.0, -1.0};
    Vec3 dir = {1.0, 1.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0));
    CHECK(hit->second == Catch::Approx(2.0));
}

TEST_CASE("aabb3_ray_miss_3d", "[spatial][aabb][3d][ray]") {
    // Ray missing the cube entirely.
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    Vec3 origin = {2.0, 2.0, -1.0};
    Vec3 dir = {0.0, 0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("aabb3_ray_origin_inside_3d", "[spatial][aabb][3d][ray]") {
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {2, 2, 2}});
    Vec3 origin = {1.0, 1.0, 1.0};
    Vec3 dir = {1.0, 0.0, 0.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first < 0.0);
    CHECK(hit->second > 0.0);
}

TEST_CASE("aabb3_ray_non_unit_direction", "[spatial][aabb][3d][ray]") {
    // Non-unit direction — t values scale accordingly.
    auto box = AABB<double, 3>::fit(std::vector<Vec3>{{0, 0, 0}, {1, 1, 1}});
    Vec3 origin = {0.5, 0.5, -2.0};
    Vec3 dir = {0.0, 0.0, 2.0}; // Twice unit speed
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0)); // t=1: z = -2 + 2*1 = 0
    CHECK(hit->second == Catch::Approx(1.5)); // t=1.5: z = -2 + 2*1.5 = 1
}

// ═══════════════════════════════════════════════════════════════════════════
// ray_intersect — KDOP<double, 3, 9> tests (non-AABB)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop9_ray_hit", "[spatial][kdop][3d][ray]") {
    // Build a 9-DOP from all 8 unit cube vertices so diagonal slabs
    // are properly expanded.
    using K9 = KDOP<double, 3, 9>;
    std::vector<Vec3> cube_verts = {{0, 0, 0},
                                    {1, 0, 0},
                                    {0, 1, 0},
                                    {0, 0, 1},
                                    {1, 1, 0},
                                    {1, 0, 1},
                                    {0, 1, 1},
                                    {1, 1, 1}};
    auto box = K9::fit(cube_verts);
    Vec3 origin = {0.5, 0.5, -1.0};
    Vec3 dir = {0.0, 0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0));
    CHECK(hit->second == Catch::Approx(2.0));
}

TEST_CASE("kdop9_ray_miss_due_to_diagonal_slab", "[spatial][kdop][3d][ray]") {
    // A ray that hits the AABB but misses the tighter 9-DOP diagonal slabs.
    // Build a 9-DOP from a single triangle (thin shape).
    using K9 = KDOP<double, 3, 9>;
    auto box = K9::fit(std::vector<Vec3>{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});

    // This ray passes through the AABB [0,1]×[0,1]×[0,0] but
    // (0.9, 0.9, 0) has x+y = 1.8, which may exceed the diagonal
    // slab max for direction (1,1,0).
    // The triangle has vertices with max x+y = 1 (at vertex (1,0,0)
    // and (0,1,0)), so slab max for (1,1,0) is 1.0.
    Vec3 origin = {0.9, 0.9, -1.0};
    Vec3 dir = {0.0, 0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    // The projection of (0.9, 0.9) onto (1,1,0) = 1.8, which exceeds
    // slab max of 1.0 → should miss.
    CHECK_FALSE(hit.has_value());
}

TEST_CASE("kdop13_ray_hit", "[spatial][kdop][3d][ray]") {
    using K13 = KDOP<double, 3, 13>;
    std::vector<Vec3> cube_verts = {{0, 0, 0},
                                    {1, 0, 0},
                                    {0, 1, 0},
                                    {0, 0, 1},
                                    {1, 1, 0},
                                    {1, 0, 1},
                                    {0, 1, 1},
                                    {1, 1, 1}};
    auto box = K13::fit(cube_verts);
    Vec3 origin = {0.5, 0.5, -1.0};
    Vec3 dir = {0.0, 0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0));
    CHECK(hit->second == Catch::Approx(2.0));
}

TEST_CASE("aabb2_ray_parallel_inside_slab", "[spatial][aabb][2d][ray]") {
    // Ray parallel to one axis, inside the other slab.
    auto box = AABB<double, 2>::fit(std::vector<Vec2>{{0, 0}, {2, 2}});
    Vec2 origin = {1.0, -1.0};
    Vec2 dir = {0.0, 1.0};
    auto hit = box.ray_intersect(origin, dir);
    REQUIRE(hit.has_value());
    CHECK(hit->first == Catch::Approx(1.0)); // Enters at y=0
    CHECK(hit->second == Catch::Approx(3.0)); // Exits at y=2
}

TEST_CASE("aabb3_ray_empty_kdop", "[spatial][aabb][3d][ray]") {
    // Ray against an empty KDOP should miss.
    auto box = AABB<double, 3>::empty();
    Vec3 origin = {0.0, 0.0, 0.0};
    Vec3 dir = {1.0, 0.0, 0.0};
    auto hit = box.ray_intersect(origin, dir);
    CHECK_FALSE(hit.has_value());
}

// ═══════════════════════════════════════════════════════════════════════════
// AABB with integral scalar type (T = uint32_t)
// ═══════════════════════════════════════════════════════════════════════════

using UVec2 = std::array<uint32_t, 2>;

TEST_CASE("aabb2_uint32_from_point", "[spatial][aabb][2d][integral]") {
    UVec2 p = {10, 20};
    auto box = AABB<uint32_t, 2>::from_point(p);

    CHECK(box.min(0) == 10);
    CHECK(box.max(0) == 10);
    CHECK(box.min(1) == 20);
    CHECK(box.max(1) == 20);
    CHECK(box.width(0) == 0);
    CHECK(box.width(1) == 0);
    CHECK(box.volume() == 0);
    CHECK(box.is_empty() == false); // Degenerate point box is not "empty".
}

TEST_CASE("aabb2_uint32_expand", "[spatial][aabb][2d][integral]") {
    UVec2 p0 = {1, 2};
    UVec2 p1 = {4, 6};
    auto box = AABB<uint32_t, 2>::from_point(p0);
    box.expand(p1);

    CHECK(box.min(0) == 1);
    CHECK(box.max(0) == 4);
    CHECK(box.min(1) == 2);
    CHECK(box.max(1) == 6);
    CHECK(box.width(0) == 3);
    CHECK(box.width(1) == 4);
    CHECK(box.volume() == 12);
}

TEST_CASE("aabb2_uint32_empty", "[spatial][aabb][2d][integral]") {
    auto box = AABB<uint32_t, 2>::empty();
    CHECK(box.is_empty());

    // Expanding with a point should produce a valid box.
    UVec2 p = {5, 10};
    box.expand(p);
    CHECK_FALSE(box.is_empty());
    CHECK(box.min(0) == 5);
    CHECK(box.max(0) == 5);
    CHECK(box.min(1) == 10);
    CHECK(box.max(1) == 10);
}

TEST_CASE("aabb2_uint32_merge", "[spatial][aabb][2d][integral]") {
    auto a = AABB<uint32_t, 2>::from_point(UVec2{0, 0});
    a.expand(UVec2{2, 3});

    auto b = AABB<uint32_t, 2>::from_point(UVec2{4, 1});
    b.expand(UVec2{6, 5});

    auto m = a.merge(b);
    CHECK(m.min(0) == 0);
    CHECK(m.max(0) == 6);
    CHECK(m.min(1) == 0);
    CHECK(m.max(1) == 5);

    // merge_in_place
    a.merge_in_place(b);
    CHECK(a.min(0) == m.min(0));
    CHECK(a.max(0) == m.max(0));
    CHECK(a.min(1) == m.min(1));
    CHECK(a.max(1) == m.max(1));
}

TEST_CASE("aabb2_uint32_contains", "[spatial][aabb][2d][integral]") {
    auto box = AABB<uint32_t, 2>::from_point(UVec2{2, 3});
    box.expand(UVec2{8, 10});

    CHECK(box.contains(UVec2{5, 5}));
    CHECK(box.contains(UVec2{2, 3})); // min corner
    CHECK(box.contains(UVec2{8, 10})); // max corner
    CHECK_FALSE(box.contains(UVec2{1, 5})); // outside left
    CHECK_FALSE(box.contains(UVec2{9, 5})); // outside right
    CHECK_FALSE(box.contains(UVec2{5, 2})); // outside bottom
    CHECK_FALSE(box.contains(UVec2{5, 11})); // outside top
}

TEST_CASE("aabb2_uint32_overlaps", "[spatial][aabb][2d][integral]") {
    auto a = AABB<uint32_t, 2>::from_point(UVec2{0, 0});
    a.expand(UVec2{4, 4});

    auto b = AABB<uint32_t, 2>::from_point(UVec2{3, 3});
    b.expand(UVec2{6, 6});

    auto c = AABB<uint32_t, 2>::from_point(UVec2{5, 5});
    c.expand(UVec2{7, 7});

    CHECK(a.overlaps(b)); // a and b overlap
    CHECK_FALSE(a.overlaps(c)); // a and c don't overlap
    CHECK(b.overlaps(c)); // b and c overlap
}

TEST_CASE("aabb2_uint32_inflate", "[spatial][aabb][2d][integral]") {
    auto box = AABB<uint32_t, 2>::from_point(UVec2{10, 10});
    box.expand(UVec2{20, 20});

    box.inflate(uint32_t{2});
    CHECK(box.min(0) == 8);
    CHECK(box.max(0) == 22);
    CHECK(box.min(1) == 8);
    CHECK(box.max(1) == 22);
}

TEST_CASE("aabb2_uint32_fit", "[spatial][aabb][2d][integral]") {
    std::vector<UVec2> pts = {{0, 0}, {100, 50}, {30, 80}};
    auto box = AABB<uint32_t, 2>::fit(pts);

    CHECK(box.min(0) == 0);
    CHECK(box.max(0) == 100);
    CHECK(box.min(1) == 0);
    CHECK(box.max(1) == 80);
    CHECK(box.volume() == 8000);
}

TEST_CASE("aabb2_uint32_equality", "[spatial][aabb][2d][integral]") {
    auto a = AABB<uint32_t, 2>::from_point(UVec2{1, 2});
    a.expand(UVec2{3, 4});

    auto b = AABB<uint32_t, 2>::from_point(UVec2{1, 2});
    b.expand(UVec2{3, 4});

    auto c = AABB<uint32_t, 2>::from_point(UVec2{1, 2});
    c.expand(UVec2{3, 5});

    CHECK(a == b);
    CHECK_FALSE(a == c);
}

TEST_CASE("aabb2_uint32_min_distance_sq", "[spatial][aabb][2d][integral]") {
    auto box = AABB<uint32_t, 2>::from_point(UVec2{10, 10});
    box.expand(UVec2{20, 20});

    // Point inside — distance is 0.
    CHECK(box.min_distance_sq(UVec2{15, 15}) == 0);

    // Point outside along x axis.
    CHECK(box.min_distance_sq(UVec2{25, 15}) == 25); // (25-20)^2 = 25

    // Point outside diagonally.
    CHECK(box.min_distance_sq(UVec2{23, 24})
          == 25); // (23-20)^2 + (24-20)^2 = 9+16 = 25
}
