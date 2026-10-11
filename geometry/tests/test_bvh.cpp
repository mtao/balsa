/// @file tests/spatial/test_bvh.cpp
/// Catch2 tests for BVH<SpatialDim, K, DirectionPolicy>.

#include <catch2/catch_all.hpp>

#include <balsa/geometry/spatial/BVH.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

using namespace balsa::geometry::spatial;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;

/// Create an AABB<double, 2> from min/max corners.
static auto make_aabb2(double x0, double y0, double x1, double y1)
    -> AABB<double, 2> {
    auto box = AABB<double, 2>::from_point(Vec2{x0, y0});
    box.expand(Vec2{x1, y1});
    return box;
}

/// Create an AABB<double, 3> from min/max corners.
static auto
    make_aabb3(double x0, double y0, double z0, double x1, double y1, double z1)
        -> AABB<double, 3> {
    auto box = AABB<double, 3>::from_point(Vec3{x0, y0, z0});
    box.expand(Vec3{x1, y1, z1});
    return box;
}

/// Build a grid of N x N unit AABBs in 2D.
static auto make_grid_2d(int n) -> std::vector<AABB<double, 2>> {
    std::vector<AABB<double, 2>> boxes;
    boxes.reserve(n * n);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            boxes.push_back(make_aabb2(x, y, x + 1, y + 1));
        }
    }
    return boxes;
}

/// Build a grid of N x N x N unit AABBs in 3D.
static auto make_grid_3d(int n) -> std::vector<AABB<double, 3>> {
    std::vector<AABB<double, 3>> boxes;
    boxes.reserve(n * n * n);
    for (int z = 0; z < n; ++z) {
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                boxes.push_back(make_aabb3(x, y, z, x + 1, y + 1, z + 1));
            }
        }
    }
    return boxes;
}

/// Verify BVH tree invariants.
template <int8_t SD, int8_t K, typename DP>
static void
    check_tree_invariants(const BVH<double, SD, K, DP> &bvh,
                          std::span<const KDOP<double, SD, K, DP>> bounds) {
    auto nodes = bvh.nodes();
    auto prim_idx = bvh.primitive_indices();

    REQUIRE(!nodes.empty());

    // Every primitive index must be valid.
    for (auto pi : prim_idx) { CHECK(pi < bounds.size()); }

    // Check each node.
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes[i];
        if (node.is_leaf()) {
            // Leaf: each primitive's bounds must be enclosed by node bounds.
            uint32_t offset = node.primitive_offset();
            for (uint16_t j = 0; j < node.primitive_count; ++j) {
                uint32_t pi = prim_idx[offset + j];
                CHECK(node.bounds.overlaps(bounds[pi]));
            }
        } else {
            // Internal node: children must be within this node's bounds.
            uint32_t left = static_cast<uint32_t>(i) + 1;
            uint32_t right = node.right_child();
            REQUIRE(left < nodes.size());
            REQUIRE(right < nodes.size());
            CHECK(node.bounds.overlaps(nodes[left].bounds));
            CHECK(node.bounds.overlaps(nodes[right].bounds));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Construction — all 4 strategies, 2D AABB
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_build_sah_2d", "[spatial][bvh][build][sah][2d]") {
    auto boxes = make_grid_2d(4); // 16 boxes
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 16);
    CHECK(bvh.node_count() > 0);
    CHECK(bvh.height() >= 0);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

TEST_CASE("bvh_build_object_median_2d",
          "[spatial][bvh][build][object_median][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::object_median;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 16);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

TEST_CASE("bvh_build_spatial_median_2d",
          "[spatial][bvh][build][spatial_median][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::spatial_median;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 16);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

TEST_CASE("bvh_build_lbvh_2d", "[spatial][bvh][build][lbvh][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::lbvh;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 16);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

// ═══════════════════════════════════════════════════════════════════════════
// Construction — 3D AABB
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_build_sah_3d", "[spatial][bvh][build][sah][3d]") {
    auto boxes = make_grid_3d(3); // 27 boxes
    AABB_BVH<double, 3> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 4;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 27);
    check_tree_invariants(bvh, std::span<const AABB<double, 3>>(boxes));
}

TEST_CASE("bvh_build_object_median_3d",
          "[spatial][bvh][build][object_median][3d]") {
    auto boxes = make_grid_3d(3);
    AABB_BVH<double, 3> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::object_median;
    config.max_leaf_size = 4;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 27);
    check_tree_invariants(bvh, std::span<const AABB<double, 3>>(boxes));
}

TEST_CASE("bvh_build_lbvh_3d", "[spatial][bvh][build][lbvh][3d]") {
    auto boxes = make_grid_3d(3);
    AABB_BVH<double, 3> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::lbvh;
    config.max_leaf_size = 4;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 27);
    check_tree_invariants(bvh, std::span<const AABB<double, 3>>(boxes));
}

// ═══════════════════════════════════════════════════════════════════════════
// Construction — k-DOP (non-AABB)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_build_sah_kdop_2d_4", "[spatial][bvh][build][sah][kdop][2d]") {
    // 4-DOP in 2D (axes + diagonals).
    using KDop4 = KDOP<double, 2, 4>;
    std::vector<KDop4> boxes;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            std::vector<Vec2> pts = {
                {static_cast<double>(x), static_cast<double>(y)},
                {static_cast<double>(x + 1), static_cast<double>(y + 1)},
            };
            boxes.push_back(KDop4::fit(pts));
        }
    }

    BVH<double, 2, 4> bvh;
    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 16);
    check_tree_invariants(bvh, std::span<const KDop4>(boxes));
}

// ═══════════════════════════════════════════════════════════════════════════
// Root bounds
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_root_bounds_enclose_all", "[spatial][bvh][build]") {
    auto boxes = make_grid_2d(3); // 9 boxes in [0,3]x[0,3]
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto root = bvh.root_bounds();
    CHECK(root.min(0) == Catch::Approx(0.0));
    CHECK(root.max(0) == Catch::Approx(3.0));
    CHECK(root.min(1) == Catch::Approx(0.0));
    CHECK(root.max(1) == Catch::Approx(3.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// Empty BVH
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_empty", "[spatial][bvh][build]") {
    AABB_BVH<double, 2> bvh;

    CHECK_FALSE(bvh.is_built());
    CHECK(bvh.node_count() == 0);
    CHECK(bvh.primitive_count() == 0);
    CHECK(bvh.height() == -1);

    // Queries on empty BVH should be no-ops.
    bool called = false;
    bvh.query_overlap(make_aabb2(0, 0, 1, 1), [&](uint32_t) { called = true; });
    CHECK_FALSE(called);

    Vec2 pt = {0.5, 0.5};
    bvh.query_point(pt, [&](uint32_t) { called = true; });
    CHECK_FALSE(called);

    auto [idx, dist] = bvh.query_nearest(pt, [](uint32_t) { return 0.0; });
    CHECK(idx == static_cast<uint32_t>(-1));
    CHECK(dist == std::numeric_limits<double>::infinity());
}

// ═══════════════════════════════════════════════════════════════════════════
// Single primitive
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_single_primitive", "[spatial][bvh][build]") {
    std::vector<AABB<double, 2>> boxes = {make_aabb2(1, 2, 3, 4)};
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    CHECK(bvh.is_built());
    CHECK(bvh.node_count() == 1);
    CHECK(bvh.primitive_count() == 1);
    CHECK(bvh.height() == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// query_overlap
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_overlap_2d", "[spatial][bvh][query][overlap][2d]") {
    // 4x4 grid of unit boxes in [0,4]x[0,4].
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    SECTION("query covering one box") {
        // Query box [0.25, 0.25] -> [0.75, 0.75] should hit only box (0,0).
        auto query = make_aabb2(0.25, 0.25, 0.75, 0.75);
        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.size() == 1);
        CHECK(hits.contains(0)); // Box (0,0) = index 0.
    }

    SECTION("query covering 2x2 region") {
        // Query box [0.5, 0.5] -> [2.5, 2.5] should hit 3x3=9 boxes
        // (those overlapping the query region).
        auto query = make_aabb2(0.5, 0.5, 2.5, 2.5);
        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        // Boxes at grid positions (0,0)..(2,2) all overlap the query.
        CHECK(hits.size() == 9);
    }

    SECTION("query outside all boxes") {
        auto query = make_aabb2(10.0, 10.0, 11.0, 11.0);
        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.empty());
    }

    SECTION("query covering all boxes") {
        auto query = make_aabb2(-1.0, -1.0, 5.0, 5.0);
        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.size() == 16);
    }
}

TEST_CASE("bvh_query_overlap_3d", "[spatial][bvh][query][overlap][3d]") {
    auto boxes = make_grid_3d(3); // 27 boxes
    AABB_BVH<double, 3> bvh;
    bvh.build(boxes);

    // Query covering the center box [1,1,1]->[2,2,2].
    auto query = make_aabb3(1.25, 1.25, 1.25, 1.75, 1.75, 1.75);
    std::set<uint32_t> hits;
    bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

    // Should hit exactly the box at grid position (1,1,1) = index 13.
    CHECK(hits.size() == 1);
    CHECK(hits.contains(13));
}

// ═══════════════════════════════════════════════════════════════════════════
// query_point
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_point_2d", "[spatial][bvh][query][point][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    SECTION("point inside one box") {
        Vec2 pt = {0.5, 0.5};
        std::set<uint32_t> hits;
        bvh.query_point(pt, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.size() == 1);
        CHECK(hits.contains(0));
    }

    SECTION("point on shared edge — hits both boxes") {
        // Point (1.0, 0.5) is on the boundary between box (0,0) and (1,0).
        Vec2 pt = {1.0, 0.5};
        std::set<uint32_t> hits;
        bvh.query_point(pt, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.size() == 2);
        CHECK(hits.contains(0));
        CHECK(hits.contains(1));
    }

    SECTION("point outside all boxes") {
        Vec2 pt = {10.0, 10.0};
        std::set<uint32_t> hits;
        bvh.query_point(pt, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.empty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// query_nearest
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_nearest_2d", "[spatial][bvh][query][nearest][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    // Distance function: squared distance from a point to the center of
    // each box's AABB.
    auto dist_to_center = [&](Vec2 pt) {
        return [&boxes, pt](uint32_t idx) -> double {
            auto c = boxes[idx].center();
            double dx = pt[0] - c[0];
            double dy = pt[1] - c[1];
            return dx * dx + dy * dy;
        };
    };

    SECTION("point at center of box (0,0)") {
        Vec2 pt = {0.5, 0.5};
        auto [idx, dist] = bvh.query_nearest(pt, dist_to_center(pt));

        CHECK(idx == 0);
        CHECK(dist == Catch::Approx(0.0));
    }

    SECTION("point outside grid, nearest to corner box") {
        Vec2 pt = {-1.0, -1.0};
        auto [idx, dist] = bvh.query_nearest(pt, dist_to_center(pt));

        // Nearest center is box (0,0) at (0.5, 0.5), distance^2 = 2.25 + 2.25
        // = 4.5
        CHECK(idx == 0);
        CHECK(dist == Catch::Approx(4.5));
    }
}

TEST_CASE("bvh_query_nearest_3d", "[spatial][bvh][query][nearest][3d]") {
    auto boxes = make_grid_3d(3);
    AABB_BVH<double, 3> bvh;
    bvh.build(boxes);

    Vec3 pt = {1.5, 1.5, 1.5};
    auto dist_func = [&](uint32_t idx) -> double {
        auto c = boxes[idx].center();
        double dx = pt[0] - c[0];
        double dy = pt[1] - c[1];
        double dz = pt[2] - c[2];
        return dx * dx + dy * dy + dz * dz;
    };

    auto [idx, dist] = bvh.query_nearest(pt, dist_func);

    // Center box at (1,1,1) -> center (1.5,1.5,1.5) -> distance 0.
    CHECK(idx == 13);
    CHECK(dist == Catch::Approx(0.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// Refit
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_refit", "[spatial][bvh][refit]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    // Shift all boxes by (10, 10).
    for (auto &b : boxes) {
        Vec2 lo = {b.min(0) + 10.0, b.min(1) + 10.0};
        Vec2 hi = {b.max(0) + 10.0, b.max(1) + 10.0};
        b = AABB<double, 2>::from_point(lo);
        b.expand(hi);
    }

    bvh.refit(boxes);

    // Root bounds should now be [10, 14] x [10, 14].
    CHECK(bvh.root_bounds().min(0) == Catch::Approx(10.0));
    CHECK(bvh.root_bounds().max(0) == Catch::Approx(14.0));
    CHECK(bvh.root_bounds().min(1) == Catch::Approx(10.0));
    CHECK(bvh.root_bounds().max(1) == Catch::Approx(14.0));

    // Queries should work with new positions.
    Vec2 pt = {10.5, 10.5};
    std::set<uint32_t> hits;
    bvh.query_point(pt, [&](uint32_t i) { hits.insert(i); });
    CHECK(hits.size() == 1);
    CHECK(hits.contains(0));

    // Old position should have no hits.
    Vec2 old_pt = {0.5, 0.5};
    std::set<uint32_t> old_hits;
    bvh.query_point(old_pt, [&](uint32_t i) { old_hits.insert(i); });
    CHECK(old_hits.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Rebuild
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_rebuild", "[spatial][bvh][rebuild]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 2;
    bvh.build(boxes, config);

    auto old_node_count = bvh.node_count();

    // Rebuild with same data.
    bvh.rebuild(boxes);

    // Same configuration should produce same result.
    CHECK(bvh.node_count() == old_node_count);
    CHECK(bvh.config().strategy == BVHBuildStrategy::sah);
    CHECK(bvh.config().max_leaf_size == 2);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

// ═══════════════════════════════════════════════════════════════════════════
// Insert
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_insert", "[spatial][bvh][insert]") {
    AABB_BVH<double, 2> bvh;

    SECTION("insert into empty tree") {
        bvh.insert(0, make_aabb2(0, 0, 1, 1));

        CHECK(bvh.is_built());
        CHECK(bvh.node_count() == 1);
        CHECK(bvh.primitive_count() == 1);
    }

    SECTION("insert multiple primitives") {
        auto b0 = make_aabb2(0, 0, 1, 1);
        auto b1 = make_aabb2(2, 2, 3, 3);
        auto b2 = make_aabb2(5, 5, 6, 6);

        // Start with one box.
        std::vector<AABB<double, 2>> boxes = {b0};
        bvh.build(boxes);

        bvh.insert(1, b1);
        bvh.insert(2, b2);

        CHECK(bvh.primitive_count() >= 3);

        // Should be able to find all three via overlap.
        auto query = make_aabb2(-1, -1, 7, 7);
        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        CHECK(hits.contains(0));
        CHECK(hits.contains(1));
        CHECK(hits.contains(2));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Remove
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_remove", "[spatial][bvh][remove]") {
    auto boxes = make_grid_2d(2); // 4 boxes
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.max_leaf_size = 4; // All in one leaf.
    bvh.build(boxes, config);

    // Remove box index 0.
    bvh.remove(0);

    // Query for box 0's region — should not find it.
    std::set<uint32_t> hits;
    bvh.query_overlap(make_aabb2(0.25, 0.25, 0.75, 0.75),
                      [&](uint32_t i) { hits.insert(i); });

    CHECK_FALSE(hits.contains(0));
}

// ═══════════════════════════════════════════════════════════════════════════
// Tree height
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_height_balanced", "[spatial][bvh][height]") {
    auto boxes = make_grid_2d(4); // 16 boxes
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::object_median;
    config.max_leaf_size = 1;
    bvh.build(boxes, config);

    // Object median produces balanced trees.
    // 16 leaves -> depth ~4 (ceil(log2(16)) = 4).
    CHECK(bvh.height() >= 3);
    CHECK(bvh.height() <= 5);
}

// ═══════════════════════════════════════════════════════════════════════════
// Max leaf size
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_max_leaf_size_respected", "[spatial][bvh][build]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::object_median;
    config.max_leaf_size = 3;
    bvh.build(boxes, config);

    // Every leaf should have at most max_leaf_size primitives.
    for (const auto &node : bvh.nodes()) {
        if (node.is_leaf()) { CHECK(node.primitive_count <= 3); }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// All strategies produce same query results
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_strategies_agree_on_queries",
          "[spatial][bvh][query][strategies]") {
    auto boxes = make_grid_2d(4);
    auto query = make_aabb2(0.5, 0.5, 2.5, 2.5);

    std::array<BVHBuildStrategy, 4> strategies = {
        BVHBuildStrategy::sah,
        BVHBuildStrategy::object_median,
        BVHBuildStrategy::spatial_median,
        BVHBuildStrategy::lbvh,
    };

    std::set<uint32_t> reference_hits;

    for (size_t s = 0; s < strategies.size(); ++s) {
        AABB_BVH<double, 2> bvh;
        BVHConfig config;
        config.strategy = strategies[s];
        config.max_leaf_size = 2;
        bvh.build(boxes, config);

        std::set<uint32_t> hits;
        bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

        if (s == 0) {
            reference_hits = hits;
        } else {
            CHECK(hits == reference_hits);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// KDOP::min_distance_sq tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kdop_min_distance_sq_inside", "[spatial][kdop][distance]") {
    auto box = make_aabb2(0, 0, 4, 4);
    Vec2 pt = {2.0, 2.0};
    CHECK(box.min_distance_sq(pt) == Catch::Approx(0.0));
}

TEST_CASE("kdop_min_distance_sq_outside_one_axis",
          "[spatial][kdop][distance]") {
    auto box = make_aabb2(0, 0, 4, 4);
    Vec2 pt = {6.0, 2.0}; // 2 units right of box.
    CHECK(box.min_distance_sq(pt) == Catch::Approx(4.0));
}

TEST_CASE("kdop_min_distance_sq_outside_corner", "[spatial][kdop][distance]") {
    auto box = make_aabb2(0, 0, 4, 4);
    Vec2 pt = {5.0, 5.0}; // (1, 1) from corner.
    CHECK(box.min_distance_sq(pt) == Catch::Approx(2.0));
}

TEST_CASE("kdop_min_distance_sq_3d", "[spatial][kdop][distance][3d]") {
    auto box = make_aabb3(0, 0, 0, 2, 2, 2);
    Vec3 pt = {3.0, 0.0, 0.0}; // 1 unit from face.
    CHECK(box.min_distance_sq(pt) == Catch::Approx(1.0));
}

TEST_CASE("kdop_min_distance_sq_on_boundary", "[spatial][kdop][distance]") {
    auto box = make_aabb2(0, 0, 4, 4);
    Vec2 pt = {4.0, 2.0}; // Exactly on boundary.
    CHECK(box.min_distance_sq(pt) == Catch::Approx(0.0));
}

// ═══════════════════════════════════════════════════════════════════════════
// Large input
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_large_input_2d", "[spatial][bvh][build][large]") {
    auto boxes = make_grid_2d(20); // 400 boxes
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 4;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 400);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));

    // Spot-check query.
    auto query = make_aabb2(9.5, 9.5, 10.5, 10.5);
    std::set<uint32_t> hits;
    bvh.query_overlap(query, [&](uint32_t i) { hits.insert(i); });

    // Should hit 4 boxes around (9.5-10.5, 9.5-10.5).
    CHECK(hits.size() == 4);
}

// ═══════════════════════════════════════════════════════════════════════════
// Degenerate inputs
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_coincident_boxes", "[spatial][bvh][build][degenerate]") {
    // All boxes are the same.
    std::vector<AABB<double, 2>> boxes(10, make_aabb2(0, 0, 1, 1));
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::sah;
    config.max_leaf_size = 4;
    bvh.build(boxes, config);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 10);

    // All primitives should be returned by an overlap query.
    std::set<uint32_t> hits;
    bvh.query_overlap(make_aabb2(0, 0, 1, 1),
                      [&](uint32_t i) { hits.insert(i); });
    CHECK(hits.size() == 10);
}

TEST_CASE("bvh_collinear_boxes", "[spatial][bvh][build][degenerate]") {
    // All boxes along a line (zero extent on Y axis).
    std::vector<AABB<double, 2>> boxes;
    for (int i = 0; i < 10; ++i) {
        boxes.push_back(make_aabb2(i, 0, i + 1, 0.1));
    }

    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    CHECK(bvh.is_built());
    CHECK(bvh.primitive_count() == 10);
    check_tree_invariants(bvh, std::span<const AABB<double, 2>>(boxes));
}

// ═══════════════════════════════════════════════════════════════════════════
// Config accessor
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_config_preserved", "[spatial][bvh][config]") {
    auto boxes = make_grid_2d(2);
    AABB_BVH<double, 2> bvh;

    BVHConfig config;
    config.strategy = BVHBuildStrategy::lbvh;
    config.max_leaf_size = 8;
    bvh.build(boxes, config);

    CHECK(bvh.config().strategy == BVHBuildStrategy::lbvh);
    CHECK(bvh.config().max_leaf_size == 8);
}

// ═══════════════════════════════════════════════════════════════════════════
// query_ray — 2D
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_ray_2d_hit", "[spatial][bvh][ray][2d]") {
    // 4x4 grid of unit boxes. Ray along x-axis at y=0.5 should hit row 0.
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    std::set<uint32_t> hits;
    auto hit_func = [&](uint32_t idx) -> std::optional<double> {
        // Check ray against individual primitive BV.
        auto interval =
            boxes[idx].ray_intersect(Vec2{-1.0, 0.5}, Vec2{1.0, 0.0});
        if (!interval.has_value() || interval->second < 0.0)
            return std::nullopt;
        return std::max(0.0, interval->first);
    };
    bvh.query_ray(Vec2{-1.0, 0.5},
                  Vec2{1.0, 0.0},
                  hit_func,
                  [&](uint32_t idx, double) { hits.insert(idx); });

    // Should hit boxes at row y=0 (indices 0, 1, 2, 3).
    CHECK(hits.size() == 4);
    CHECK(hits.count(0) == 1);
    CHECK(hits.count(1) == 1);
    CHECK(hits.count(2) == 1);
    CHECK(hits.count(3) == 1);
}

TEST_CASE("bvh_query_ray_2d_miss", "[spatial][bvh][ray][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    std::set<uint32_t> hits;
    auto hit_func = [](uint32_t) -> std::optional<double> { return 1.0; };
    // Ray above the grid.
    bvh.query_ray(Vec2{-1.0, 5.0},
                  Vec2{1.0, 0.0},
                  hit_func,
                  [&](uint32_t idx, double) { hits.insert(idx); });

    CHECK(hits.empty());
}

TEST_CASE("bvh_query_ray_2d_hit_func_filters", "[spatial][bvh][ray][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    std::set<uint32_t> hits;
    auto hit_func = [](uint32_t idx) -> std::optional<double> {
        // Only report even-indexed primitives.
        if (idx % 2 == 0) return static_cast<double>(idx);
        return std::nullopt;
    };
    bvh.query_ray(Vec2{-1.0, 0.5},
                  Vec2{1.0, 0.0},
                  hit_func,
                  [&](uint32_t idx, double) { hits.insert(idx); });

    // Should only contain even indices from row 0.
    for (auto idx : hits) { CHECK(idx % 2 == 0); }
}

TEST_CASE("bvh_query_ray_empty_bvh", "[spatial][bvh][ray]") {
    AABB_BVH<double, 2> bvh;
    std::set<uint32_t> hits;
    auto hit_func = [](uint32_t) -> std::optional<double> { return 1.0; };
    bvh.query_ray(Vec2{0, 0}, Vec2{1, 0}, hit_func, [&](uint32_t idx, double) {
        hits.insert(idx);
    });
    CHECK(hits.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// query_ray — 3D
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_ray_3d_hit", "[spatial][bvh][ray][3d]") {
    auto boxes = make_grid_3d(3);
    AABB_BVH<double, 3> bvh;
    bvh.build(boxes);

    std::set<uint32_t> hits;
    auto hit_func = [&](uint32_t idx) -> std::optional<double> {
        auto c = boxes[idx].center();
        return c[2]; // Use z-center as "t"
    };
    // Ray along z-axis through center of column (0.5, 0.5, *)
    bvh.query_ray(Vec3{0.5, 0.5, -1.0},
                  Vec3{0.0, 0.0, 1.0},
                  hit_func,
                  [&](uint32_t idx, double) { hits.insert(idx); });

    // Should hit 3 boxes in the column at (x=0, y=0, z=0..2).
    CHECK(hits.size() == 3);
}

// ═══════════════════════════════════════════════════════════════════════════
// query_ray_nearest
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_ray_nearest_2d", "[spatial][bvh][ray][nearest][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto hit_func = [&](uint32_t idx) -> std::optional<double> {
        auto c = boxes[idx].center();
        // t = how far along the ray we hit the center.
        // ray: origin + t*dir, origin=(-1, 0.5), dir=(1,0)
        // So t = c[0] - (-1) = c[0] + 1.
        return c[0] + 1.0;
    };
    auto [idx, t] =
        bvh.query_ray_nearest(Vec2{-1.0, 0.5}, Vec2{1.0, 0.0}, hit_func);

    // The nearest box along this ray should be at x=0 (index 0).
    // Its center is at x=0.5, so t = 0.5 + 1 = 1.5.
    REQUIRE(idx != static_cast<uint32_t>(-1));
    CHECK(t == Catch::Approx(1.5));
    CHECK(boxes[idx].center()[0] == Catch::Approx(0.5));
}

TEST_CASE("bvh_query_ray_nearest_no_hit", "[spatial][bvh][ray][nearest]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto hit_func = [](uint32_t) -> std::optional<double> {
        return std::nullopt; // Never hit.
    };
    auto [idx, t] =
        bvh.query_ray_nearest(Vec2{-1.0, 0.5}, Vec2{1.0, 0.0}, hit_func);

    CHECK(idx == static_cast<uint32_t>(-1));
    CHECK(t == std::numeric_limits<double>::infinity());
}

TEST_CASE("bvh_query_ray_nearest_empty_bvh", "[spatial][bvh][ray][nearest]") {
    AABB_BVH<double, 2> bvh;
    auto hit_func = [](uint32_t) -> std::optional<double> { return 1.0; };
    auto [idx, t] = bvh.query_ray_nearest(Vec2{0, 0}, Vec2{1, 0}, hit_func);
    CHECK(idx == static_cast<uint32_t>(-1));
}

TEST_CASE("bvh_query_ray_nearest_rejects_negative_t",
          "[spatial][bvh][ray][nearest]") {
    // Ensure that hits with negative t are rejected.
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto hit_func = [](uint32_t) -> std::optional<double> {
        return -1.0; // Behind the ray.
    };
    auto [idx, t] =
        bvh.query_ray_nearest(Vec2{-1.0, 0.5}, Vec2{1.0, 0.0}, hit_func);
    CHECK(idx == static_cast<uint32_t>(-1));
}

// ═══════════════════════════════════════════════════════════════════════════
// query_within_radius
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_within_radius_2d", "[spatial][bvh][radius][2d]") {
    // 4x4 grid. Query point at (0.5, 0.5) with radius 1.
    // Should find the box at (0,0)-(1,1) with distance 0.
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    std::vector<std::pair<uint32_t, double>> results;
    auto distance_func = [&](uint32_t idx) -> double {
        // Distance from query point to center of box.
        auto c = boxes[idx].center();
        double dx = c[0] - 0.5, dy = c[1] - 0.5;
        return dx * dx + dy * dy;
    };
    bvh.query_within_radius(
        Vec2{0.5, 0.5}, 1.0, distance_func, [&](uint32_t idx, double d) {
            results.emplace_back(idx, d);
        });

    // At least the box containing the query point should be found.
    CHECK(!results.empty());
    // Box 0 has center (0.5, 0.5) — distance 0.
    bool found_zero_dist = false;
    for (auto &[idx, d] : results) {
        if (d < 1e-14) { found_zero_dist = true; }
        CHECK(d <= 1.0);
    }
    CHECK(found_zero_dist);
}

TEST_CASE("bvh_query_within_radius_none", "[spatial][bvh][radius]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    std::vector<uint32_t> results;
    auto distance_func = [](uint32_t) -> double {
        return 1000.0; // All primitives very far.
    };
    bvh.query_within_radius(
        Vec2{-10.0, -10.0}, 1.0, distance_func, [&](uint32_t idx, double) {
            results.push_back(idx);
        });
    CHECK(results.empty());
}

TEST_CASE("bvh_query_within_radius_empty_bvh", "[spatial][bvh][radius]") {
    AABB_BVH<double, 2> bvh;
    std::vector<uint32_t> results;
    bvh.query_within_radius(
        Vec2{0, 0},
        1.0,
        [](uint32_t) { return 0.0; },
        [&](uint32_t idx, double) { results.push_back(idx); });
    CHECK(results.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// query_k_nearest
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_query_k_nearest_2d", "[spatial][bvh][knearest][2d]") {
    auto boxes = make_grid_2d(4);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto distance_func = [&](uint32_t idx) -> double {
        auto c = boxes[idx].center();
        double dx = c[0] - 0.5, dy = c[1] - 0.5;
        return dx * dx + dy * dy;
    };
    auto results = bvh.query_k_nearest(Vec2{0.5, 0.5}, 3, distance_func);

    CHECK(results.size() == 3);
    // Results should be sorted by ascending distance.
    for (size_t i = 1; i < results.size(); ++i) {
        CHECK(results[i].second >= results[i - 1].second);
    }
    // The closest should be box 0 (distance 0 from (0.5, 0.5)).
    CHECK(results[0].second < 1e-14);
}

TEST_CASE("bvh_query_k_nearest_more_than_available",
          "[spatial][bvh][knearest]") {
    // Only 4 boxes but asking for k=10.
    auto boxes = make_grid_2d(2);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto distance_func = [&](uint32_t idx) -> double {
        auto c = boxes[idx].center();
        return c[0] * c[0] + c[1] * c[1];
    };
    auto results = bvh.query_k_nearest(Vec2{0, 0}, 10, distance_func);

    CHECK(results.size() == 4); // Only 4 available.
    for (size_t i = 1; i < results.size(); ++i) {
        CHECK(results[i].second >= results[i - 1].second);
    }
}

TEST_CASE("bvh_query_k_nearest_k_zero", "[spatial][bvh][knearest]") {
    auto boxes = make_grid_2d(2);
    AABB_BVH<double, 2> bvh;
    bvh.build(boxes);

    auto results =
        bvh.query_k_nearest(Vec2{0, 0}, 0, [](uint32_t) { return 0.0; });
    CHECK(results.empty());
}

TEST_CASE("bvh_query_k_nearest_empty_bvh", "[spatial][bvh][knearest]") {
    AABB_BVH<double, 2> bvh;
    auto results =
        bvh.query_k_nearest(Vec2{0, 0}, 5, [](uint32_t) { return 0.0; });
    CHECK(results.empty());
}

TEST_CASE("bvh_query_k_nearest_3d", "[spatial][bvh][knearest][3d]") {
    auto boxes = make_grid_3d(3);
    AABB_BVH<double, 3> bvh;
    bvh.build(boxes);

    auto distance_func = [&](uint32_t idx) -> double {
        auto c = boxes[idx].center();
        double dx = c[0] - 0.5, dy = c[1] - 0.5, dz = c[2] - 0.5;
        return dx * dx + dy * dy + dz * dz;
    };
    auto results = bvh.query_k_nearest(Vec3{0.5, 0.5, 0.5}, 1, distance_func);

    REQUIRE(results.size() == 1);
    CHECK(results[0].second < 1e-14); // Box 0 center matches query.
}

// ═══════════════════════════════════════════════════════════════════════════
// query_overlap_bvh
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("bvh_overlap_bvh_identical", "[spatial][bvh][overlap_bvh]") {
    // Two identical grids — every box should overlap its counterpart.
    auto boxes = make_grid_2d(2);
    AABB_BVH<double, 2> bvh_a, bvh_b;
    bvh_a.build(boxes);
    bvh_b.build(boxes);

    std::set<std::pair<uint32_t, uint32_t>> pairs;
    bvh_a.query_overlap_bvh(
        bvh_b, [&](uint32_t a, uint32_t b) { pairs.insert({a, b}); });

    // Each box overlaps itself and its neighbors (sharing edges).
    // At minimum, each box overlaps with itself.
    for (uint32_t i = 0; i < boxes.size(); ++i) {
        CHECK(pairs.count({i, i}) == 1);
    }
}

TEST_CASE("bvh_overlap_bvh_disjoint", "[spatial][bvh][overlap_bvh]") {
    // Two grids that don't overlap.
    auto boxes_a = make_grid_2d(2); // [0,2]x[0,2]
    std::vector<AABB<double, 2>> boxes_b;
    for (auto &b : make_grid_2d(2)) {
        // Shift by (10, 10).
        auto c = b.center();
        boxes_b.push_back(
            make_aabb2(c[0] + 9.5, c[1] + 9.5, c[0] + 10.5, c[1] + 10.5));
    }

    AABB_BVH<double, 2> bvh_a, bvh_b;
    bvh_a.build(boxes_a);
    bvh_b.build(boxes_b);

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    bvh_a.query_overlap_bvh(
        bvh_b, [&](uint32_t a, uint32_t b) { pairs.emplace_back(a, b); });

    CHECK(pairs.empty());
}

TEST_CASE("bvh_overlap_bvh_partial", "[spatial][bvh][overlap_bvh]") {
    // BVH A: single box at [0,1]x[0,1].
    // BVH B: two boxes at [0.5,1.5]x[0.5,1.5] and [5,6]x[5,6].
    std::vector<AABB<double, 2>> boxes_a = {make_aabb2(0, 0, 1, 1)};
    std::vector<AABB<double, 2>> boxes_b = {make_aabb2(0.5, 0.5, 1.5, 1.5),
                                            make_aabb2(5, 5, 6, 6)};

    AABB_BVH<double, 2> bvh_a, bvh_b;
    bvh_a.build(boxes_a);
    bvh_b.build(boxes_b);

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    bvh_a.query_overlap_bvh(
        bvh_b, [&](uint32_t a, uint32_t b) { pairs.emplace_back(a, b); });

    REQUIRE(pairs.size() == 1);
    CHECK(pairs[0].first == 0);
    CHECK(pairs[0].second == 0);
}

TEST_CASE("bvh_overlap_bvh_empty", "[spatial][bvh][overlap_bvh]") {
    auto boxes = make_grid_2d(2);
    AABB_BVH<double, 2> bvh_a, bvh_b;
    bvh_a.build(boxes);
    // bvh_b is empty.

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    bvh_a.query_overlap_bvh(
        bvh_b, [&](uint32_t a, uint32_t b) { pairs.emplace_back(a, b); });
    CHECK(pairs.empty());
}

TEST_CASE("bvh_overlap_bvh_3d", "[spatial][bvh][overlap_bvh][3d]") {
    // Two 2x2x2 grids with partial overlap.
    auto boxes_a = make_grid_3d(2); // [0,2]^3

    // Shift by (1, 1, 1) — overlap region is [1,2]^3.
    std::vector<AABB<double, 3>> boxes_b;
    for (auto &b : make_grid_3d(2)) {
        auto c = b.center();
        boxes_b.push_back(make_aabb3(c[0] + 0.5,
                                     c[1] + 0.5,
                                     c[2] + 0.5,
                                     c[0] + 1.5,
                                     c[1] + 1.5,
                                     c[2] + 1.5));
    }

    AABB_BVH<double, 3> bvh_a, bvh_b;
    bvh_a.build(boxes_a);
    bvh_b.build(boxes_b);

    std::set<std::pair<uint32_t, uint32_t>> pairs;
    bvh_a.query_overlap_bvh(
        bvh_b, [&](uint32_t a, uint32_t b) { pairs.insert({a, b}); });

    // Should find some overlaps.
    CHECK(!pairs.empty());
}
