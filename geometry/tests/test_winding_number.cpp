#include <catch2/catch_all.hpp>
#include <iostream>
#include <balsa/geometry/winding_number.hpp>
#include <balsa/zipper/types.hpp>
#include <map>
#include <random>
#include <vector>

using namespace balsa::geometry;

TEST_CASE("winding numbers", "[winding_number]") {
    // Unit square vertices: (0,0), (1,0), (1,1), (0,1)
    balsa::zipper::ColVecs2d V(2, 4);
    V.col(0) = balsa::zipper::Vec2d{0.0, 0.0};
    V.col(1) = balsa::zipper::Vec2d{1.0, 0.0};
    V.col(2) = balsa::zipper::Vec2d{1.0, 1.0};
    V.col(3) = balsa::zipper::Vec2d{0.0, 1.0};

    // CCW curve through vertex indices 0->1->2->3 (wraps back to 0)
    std::vector<int> curve{ 0, 1, 2, 3 };

    // Edge list for the same square
    balsa::zipper::ColVecs2i E(2, 4);
    E.col(0) = balsa::zipper::Vec2i{0, 1};
    E.col(1) = balsa::zipper::Vec2i{1, 2};
    E.col(2) = balsa::zipper::Vec2i{2, 3};
    E.col(3) = balsa::zipper::Vec2i{3, 0};

    // Boundary map: all edges with positive orientation
    std::map<int, bool> B;
    B[0] = true;
    B[1] = true;
    B[2] = true;
    B[3] = true;

    SECTION("interior point has winding number ~1") {
        balsa::zipper::Vec2d p{0.5, 0.5};

        auto wn_curve = winding_number(V, curve, p);
        CHECK(std::abs(wn_curve - 1.0) < 1e-10);

        auto wn_mesh = mesh_winding_number(V, E, p);
        CHECK(std::abs(wn_mesh - 1.0) < 1e-10);

        auto wn_boundary = mesh_winding_number(V, E, B, p);
        CHECK(std::abs(wn_boundary - 1.0) < 1e-10);

        CHECK(interior_mesh_winding_number(V, E, p));
        CHECK(interior_mesh_winding_number(V, E, B, p));
    }

    SECTION("exterior point has winding number ~0") {
        balsa::zipper::Vec2d p{2.0, 2.0};

        auto wn_curve = winding_number(V, curve, p);
        CHECK(std::abs(wn_curve) < 1e-10);

        auto wn_mesh = mesh_winding_number(V, E, p);
        CHECK(std::abs(wn_mesh) < 1e-10);

        auto wn_boundary = mesh_winding_number(V, E, B, p);
        CHECK(std::abs(wn_boundary) < 1e-10);

        CHECK_FALSE(interior_mesh_winding_number(V, E, p));
        CHECK_FALSE(interior_mesh_winding_number(V, E, B, p));
    }

    SECTION("another exterior point - negative quadrant") {
        balsa::zipper::Vec2d p{-1.0, -1.0};

        auto wn_curve = winding_number(V, curve, p);
        CHECK(std::abs(wn_curve) < 1e-10);

        CHECK_FALSE(interior_mesh_winding_number(V, E, p));
    }

    SECTION("point near edge - just inside") {
        balsa::zipper::Vec2d p{0.5, 0.01};

        auto wn = mesh_winding_number(V, E, p);
        CHECK(std::abs(wn - 1.0) < 1e-5);
        CHECK(interior_mesh_winding_number(V, E, p));
    }

    SECTION("point near edge - just outside") {
        balsa::zipper::Vec2d p{0.5, -0.01};

        auto wn = mesh_winding_number(V, E, p);
        CHECK(std::abs(wn) < 1e-5);
        CHECK_FALSE(interior_mesh_winding_number(V, E, p));
    }

    SECTION("consistency across all three methods for random points") {
        std::mt19937 gen(0);
        std::uniform_real_distribution<double> dist(-1.0, 2.0);
        for (int i = 0; i < 20; ++i) {
            // Random points in [-1, 2] range (some inside, some outside)
            balsa::zipper::Vec2d p{dist(gen), dist(gen)};

            auto wn_curve = winding_number(V, curve, p);
            auto wn_mesh = mesh_winding_number(V, E, p);
            auto wn_boundary = mesh_winding_number(V, E, B, p);

            // All three methods should agree
            CHECK(std::abs(wn_curve - wn_mesh) < 1e-10);
            CHECK(std::abs(wn_curve - wn_boundary) < 1e-10);
        }
    }
}
