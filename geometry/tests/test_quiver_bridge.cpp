#include <catch2/catch_all.hpp>
#include <array>
#include <cmath>
#include <vector>

#include <balsa/geometry/quiver/to_col_vectors.hpp>
#include <balsa/geometry/quiver/simplex_transforms.hpp>
#include <balsa/utils/factorial.hpp>
#include <quiver/attributes/IncidentData.hpp>
#include <quiver/topology/SimplexIndices.hpp>

using namespace Catch;
namespace bgq = balsa::geometry::quiver;

// ── to_col_vectors with ReferencingStorage ────────────────────────────────

TEST_CASE("to_col_vectors referencing storage 2D triangle",
          "[quiver_bridge]") {
    // Three 2D points forming a right triangle at the origin
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto M = bgq::to_col_vectors(id);

    // Result should be a 2x3 matrix
    REQUIRE(M.extent(0) == 2);
    REQUIRE(M.extent(1) == 3);

    // Column 0 = (0, 0)
    CHECK(M(0, 0) == Approx(0.0));
    CHECK(M(1, 0) == Approx(0.0));
    // Column 1 = (1, 0)
    CHECK(M(0, 1) == Approx(1.0));
    CHECK(M(1, 1) == Approx(0.0));
    // Column 2 = (0, 1)
    CHECK(M(0, 2) == Approx(0.0));
    CHECK(M(1, 2) == Approx(1.0));
}

TEST_CASE("to_col_vectors referencing storage 3D triangle",
          "[quiver_bridge]") {
    std::vector<std::array<double, 3>> positions = {
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto M = bgq::to_col_vectors(id);

    REQUIRE(M.extent(0) == 3);
    REQUIRE(M.extent(1) == 3);

    CHECK(M(0, 0) == Approx(1.0));
    CHECK(M(1, 0) == Approx(0.0));
    CHECK(M(2, 0) == Approx(0.0));

    CHECK(M(0, 1) == Approx(0.0));
    CHECK(M(1, 1) == Approx(1.0));
    CHECK(M(2, 1) == Approx(0.0));

    CHECK(M(0, 2) == Approx(0.0));
    CHECK(M(1, 2) == Approx(0.0));
    CHECK(M(2, 2) == Approx(1.0));
}

TEST_CASE("to_col_vectors referencing with scattered indices",
          "[quiver_bridge]") {
    // Non-contiguous index pattern
    std::vector<std::array<double, 2>> positions = {
        { 10.0, 20.0 },
        { 30.0, 40.0 },
        { 50.0, 60.0 },
        { 70.0, 80.0 },
        { 90.0, 100.0 }
    };
    std::array<int64_t, 3> indices = { 4, 1, 3 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto M = bgq::to_col_vectors(id);

    REQUIRE(M.extent(0) == 2);
    REQUIRE(M.extent(1) == 3);

    // Column 0 = positions[4] = (90, 100)
    CHECK(M(0, 0) == Approx(90.0));
    CHECK(M(1, 0) == Approx(100.0));
    // Column 1 = positions[1] = (30, 40)
    CHECK(M(0, 1) == Approx(30.0));
    CHECK(M(1, 1) == Approx(40.0));
    // Column 2 = positions[3] = (70, 80)
    CHECK(M(0, 2) == Approx(70.0));
    CHECK(M(1, 2) == Approx(80.0));
}


// ── to_col_vectors with OwningStorage ─────────────────────────────────────

TEST_CASE("to_col_vectors owning storage", "[quiver_bridge]") {
    std::array<std::array<double, 2>, 3> values = {
        std::array{ 1.0, 2.0 },
        std::array{ 3.0, 4.0 },
        std::array{ 5.0, 6.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(values, indices);
    auto M = bgq::to_col_vectors(id);

    REQUIRE(M.extent(0) == 2);
    REQUIRE(M.extent(1) == 3);

    CHECK(M(0, 0) == Approx(1.0));
    CHECK(M(1, 0) == Approx(2.0));
    CHECK(M(0, 1) == Approx(3.0));
    CHECK(M(1, 1) == Approx(4.0));
    CHECK(M(0, 2) == Approx(5.0));
    CHECK(M(1, 2) == Approx(6.0));
}


// ── to_std_array round-trip ───────────────────────────────────────────────

TEST_CASE("to_std_array round-trip", "[quiver_bridge]") {
    std::array<double, 3> original = { 1.5, 2.5, 3.5 };

    // Create a zipper Vector from the array values, then convert back
    ::zipper::Vector<double, 3> v(3);
    for (size_t i = 0; i < 3; ++i) {
        v(i) = original[i];
    }
    auto result = bgq::to_std_array(v);

    static_assert(std::is_same_v<decltype(result), std::array<double, 3>>);
    CHECK(result[0] == Approx(1.5));
    CHECK(result[1] == Approx(2.5));
    CHECK(result[2] == Approx(3.5));
}


// ── simplex_col_vectors with standalone SimplexIndices ────────────────────

TEST_CASE("simplex_col_vectors standalone SimplexIndices",
          "[quiver_bridge]") {
    // Two 2D triangles: {0,1,2} and {1,2,3}
    using SI = quiver::topology::SimplexIndices<0, 2>;
    std::vector<SI::attribute_value_type> tri_data = {
        { 0, 1, 2 },
        { 1, 2, 3 }
    };

    SI si(std::move(tri_data));
    REQUIRE(si.valid());

    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 },
        { 1.0, 1.0 }
    };

    // First triangle: vertices 0, 1, 2
    auto M0 = bgq::simplex_col_vectors<0, 2>(si, positions.data(), 0);
    REQUIRE(M0.extent(0) == 2);
    REQUIRE(M0.extent(1) == 3);

    CHECK(M0(0, 0) == Approx(0.0));
    CHECK(M0(1, 0) == Approx(0.0));
    CHECK(M0(0, 1) == Approx(1.0));
    CHECK(M0(1, 1) == Approx(0.0));
    CHECK(M0(0, 2) == Approx(0.0));
    CHECK(M0(1, 2) == Approx(1.0));

    // Second triangle: vertices 1, 2, 3
    auto M1 = bgq::simplex_col_vectors<0, 2>(si, positions.data(), 1);
    CHECK(M1(0, 0) == Approx(1.0));
    CHECK(M1(1, 0) == Approx(0.0));
    CHECK(M1(0, 1) == Approx(0.0));
    CHECK(M1(1, 1) == Approx(1.0));
    CHECK(M1(0, 2) == Approx(1.0));
    CHECK(M1(1, 2) == Approx(1.0));
}

TEST_CASE("simplex_col_vectors 3D tetrahedra", "[quiver_bridge]") {
    // Single tet in 3D: vertices {0,1,2,3}
    using SI = quiver::topology::SimplexIndices<0, 3>;
    std::vector<SI::attribute_value_type> tet_data = { { 0, 1, 2, 3 } };

    SI si(std::move(tet_data));

    std::vector<std::array<double, 3>> positions = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };

    auto M = bgq::simplex_col_vectors<0, 3>(si, positions.data(), 0);
    REQUIRE(M.extent(0) == 3);
    REQUIRE(M.extent(1) == 4);

    // Column 0 = origin
    CHECK(M(0, 0) == Approx(0.0));
    CHECK(M(1, 0) == Approx(0.0));
    CHECK(M(2, 0) == Approx(0.0));
    // Column 1 = (1,0,0)
    CHECK(M(0, 1) == Approx(1.0));
    CHECK(M(1, 1) == Approx(0.0));
    CHECK(M(2, 1) == Approx(0.0));
    // Column 2 = (0,1,0)
    CHECK(M(0, 2) == Approx(0.0));
    CHECK(M(1, 2) == Approx(1.0));
    CHECK(M(2, 2) == Approx(0.0));
    // Column 3 = (0,0,1)
    CHECK(M(0, 3) == Approx(0.0));
    CHECK(M(1, 3) == Approx(0.0));
    CHECK(M(2, 3) == Approx(1.0));
}


// ── VolumeTransform ───────────────────────────────────────────────────────

TEST_CASE("VolumeTransform 2D right triangle", "[quiver_bridge]") {
    // 2D right triangle at origin: area = 0.5, signed volume = 0.5
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    // 2x3 matrix: rows+1 == cols, so signed volume = det / 2! = 0.5
    CHECK(vol == Approx(0.5));
}

TEST_CASE("VolumeTransform 2D scaled triangle", "[quiver_bridge]") {
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 2.0, 0.0 },
        { 0.0, 3.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    // area = 0.5 * 2 * 3 = 3.0
    CHECK(vol == Approx(3.0));
}

TEST_CASE("VolumeTransform 3D triangle (unsigned)", "[quiver_bridge]") {
    // 3D triangle: rows=3, cols=3, rows+1 != cols -> unsigned volume
    std::vector<std::array<double, 3>> positions = {
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    // This is the area of the triangle with vertices on the unit axes
    // Edge vectors: (-1,1,0) and (-1,0,1)
    // Cross product magnitude = sqrt(3), area = sqrt(3)/2
    CHECK(vol == Approx(std::sqrt(3.0) / 2.0));
}

TEST_CASE("VolumeTransform 3D tetrahedron", "[quiver_bridge]") {
    // 3D tet: rows=3, cols=4, rows+1 == cols -> signed volume
    std::vector<std::array<double, 3>> positions = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };
    std::array<int64_t, 4> indices = { 0, 1, 2, 3 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    // det / 3! = 1.0 / 6.0
    CHECK(vol == Approx(1.0 / 6.0));
}

TEST_CASE("VolumeTransform identity simplex parametric",
          "[quiver_bridge]") {
    // Test the standard identity simplex pattern in 2D, 3D, 4D
    auto test_identity = []<int N>(std::integral_constant<int, N>) {
        // N-dimensional simplex with N+1 vertices:
        // vertex 0 at origin, vertex j at e_j (j=1..N)
        std::vector<std::array<double, N>> positions(N + 1);
        positions[0].fill(0.0);
        for (int j = 1; j <= N; ++j) {
            positions[j].fill(0.0);
            positions[j][j - 1] = 1.0;
        }

        std::array<int64_t, N + 1> indices;
        for (int j = 0; j <= N; ++j) {
            indices[j] = j;
        }

        auto id = bgq::make_incident_data(positions.data(), indices);
        double vol = bgq::VolumeTransform{}(id);

        CHECK(vol == Approx(1.0 / balsa::utils::factorial(N)));
    };

    test_identity(std::integral_constant<int, 2>{});
    test_identity(std::integral_constant<int, 3>{});
    test_identity(std::integral_constant<int, 4>{});
}


// ── CircumcenterTransform ─────────────────────────────────────────────────

TEST_CASE("CircumcenterTransform 2D right triangle", "[quiver_bridge]") {
    // Right triangle: (0,0), (1,0), (0,1)
    // Circumcenter of a right triangle is at the midpoint of the hypotenuse
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto C = bgq::CircumcenterTransform{}(id);

    static_assert(std::is_same_v<decltype(C), std::array<double, 2>>);
    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(0.5));
}

TEST_CASE("CircumcenterTransform equilateral triangle", "[quiver_bridge]") {
    // Equilateral triangle centered (roughly) around its centroid
    double h = std::sqrt(3.0) / 2.0;
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.5, h }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto C = bgq::CircumcenterTransform{}(id);

    // Circumcenter of equilateral triangle = centroid
    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(h / 3.0));
}

TEST_CASE("CircumcenterTransform 3D tet", "[quiver_bridge]") {
    // Regular tet at origin: circumcenter at (1/3, 1/3, 1/3) ... no.
    // Standard tet: (0,0,0), (1,0,0), (0,1,0), (0,0,1)
    // Circumcenter: all vertices equidistant from center.
    // By symmetry of the right-angle tet, circumcenter = (0.5, 0.5, 0.5)
    // Check: dist from (0,0,0) = sqrt(0.75), dist from (1,0,0) = sqrt(0.25+0.25+0.25) = sqrt(0.75). Correct.
    std::vector<std::array<double, 3>> positions = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };
    std::array<int64_t, 4> indices = { 0, 1, 2, 3 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto C = bgq::CircumcenterTransform{}(id);

    static_assert(std::is_same_v<decltype(C), std::array<double, 3>>);
    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(0.5));
    CHECK(C[2] == Approx(0.5));
}

TEST_CASE("CircumcenterTransform 3D triangle (SPSD path)",
          "[quiver_bridge]") {
    // 3D triangle: 3 rows, 3 cols -> not SPD (rows+1 != cols)
    // Uses SPSD (QR) solver path.
    // Triangle on the XY plane: circumcenter should have z=0
    std::vector<std::array<double, 3>> positions = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto C = bgq::CircumcenterTransform{}(id);

    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(0.5));
    CHECK(C[2] == Approx(0.0));
}


// ── CircumcenterWithSquaredRadiusTransform ────────────────────────────────

TEST_CASE("CircumcenterWithSquaredRadiusTransform", "[quiver_bridge]") {
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto [C, r2] = bgq::CircumcenterWithSquaredRadiusTransform{}(id);

    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(0.5));
    // Distance from (0.5, 0.5) to (0, 0) = sqrt(0.5), r2 = 0.5
    CHECK(r2 == Approx(0.5));
}


// ── CircumcenterWithRadiusTransform ───────────────────────────────────────

TEST_CASE("CircumcenterWithRadiusTransform", "[quiver_bridge]") {
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 1.0, 0.0 },
        { 0.0, 1.0 }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto [C, r] = bgq::CircumcenterWithRadiusTransform{}(id);

    CHECK(C[0] == Approx(0.5));
    CHECK(C[1] == Approx(0.5));
    CHECK(r == Approx(std::sqrt(0.5)));
}


// ── Equidistance property of circumcenter via bridge ──────────────────────

TEST_CASE("circumcenter equidistance via bridge", "[quiver_bridge]") {
    // All vertices should be equidistant from the circumcenter
    auto test = []<int N>(std::integral_constant<int, N>) {
        // Identity simplex in N dimensions
        std::vector<std::array<double, N>> positions(N + 1);
        positions[0].fill(0.0);
        for (int j = 1; j <= N; ++j) {
            positions[j].fill(0.0);
            positions[j][j - 1] = 1.0;
        }

        std::array<int64_t, N + 1> indices;
        for (int j = 0; j <= N; ++j) {
            indices[j] = j;
        }

        auto id = bgq::make_incident_data(positions.data(), indices);
        auto [C, r] = bgq::CircumcenterWithRadiusTransform{}(id);

        // Check all vertices are at distance r from C
        for (int v = 0; v <= N; ++v) {
            double dist2 = 0;
            for (int d = 0; d < N; ++d) {
                double diff = positions[v][d] - C[d];
                dist2 += diff * diff;
            }
            CHECK(std::sqrt(dist2) == Approx(r));
        }
    };

    test(std::integral_constant<int, 2>{});
    test(std::integral_constant<int, 3>{});
    test(std::integral_constant<int, 4>{});
}


// ── SimplexIndices owning constructor standalone test ──────────────────────

TEST_CASE("SimplexIndices owning constructor", "[quiver_bridge]") {
    using SI = quiver::topology::SimplexIndices<0, 2>;
    static_assert(SI::total_indices == 3);

    std::vector<SI::attribute_value_type> data = {
        { 0, 1, 2 },
        { 1, 2, 3 },
        { 2, 3, 4 }
    };

    SI si(data);
    REQUIRE(si.valid());

    // Access through handle
    CHECK(si.handle()[0] == SI::attribute_value_type{ 0, 1, 2 });
    CHECK(si.handle()[1] == SI::attribute_value_type{ 1, 2, 3 });
    CHECK(si.handle()[2] == SI::attribute_value_type{ 2, 3, 4 });

    // Copy should share ownership (shared_ptr semantics)
    SI si_copy = si;
    REQUIRE(si_copy.valid());
    CHECK(si_copy.handle()[0] == SI::attribute_value_type{ 0, 1, 2 });
}

TEST_CASE("SimplexIndices owning constructor 3D tet", "[quiver_bridge]") {
    using SI = quiver::topology::SimplexIndices<0, 3>;
    static_assert(SI::total_indices == 4);

    std::vector<SI::attribute_value_type> data = { { 0, 1, 2, 3 } };

    SI si(std::move(data));
    REQUIRE(si.valid());
    CHECK(si.handle()[0] == SI::attribute_value_type{ 0, 1, 2, 3 });
}


// ── Float scalar type ─────────────────────────────────────────────────────

TEST_CASE("bridge with float scalars", "[quiver_bridge]") {
    std::vector<std::array<float, 2>> positions = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 0.0f, 1.0f }
    };
    std::array<int64_t, 3> indices = { 0, 1, 2 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto M = bgq::to_col_vectors(id);

    static_assert(std::is_same_v<typename decltype(M)::value_type, float>);
    REQUIRE(M.extent(0) == 2);
    REQUIRE(M.extent(1) == 3);

    CHECK(M(0, 0) == Approx(0.0f));
    CHECK(M(0, 1) == Approx(1.0f));

    float vol = bgq::VolumeTransform{}(id);
    CHECK(vol == Approx(0.5f));
}


// ── Edge (2-vertex simplex) ───────────────────────────────────────────────

TEST_CASE("VolumeTransform 2D edge", "[quiver_bridge]") {
    // 2D edge: 2 rows, 2 cols -> rows+1 != cols -> unsigned volume (= length)
    // Wait: rows=2, cols=2, rows+1=3 != 2. So unsigned.
    // Edge from (0,0) to (3,4): length = 5
    std::vector<std::array<double, 2>> positions = {
        { 0.0, 0.0 },
        { 3.0, 4.0 }
    };
    std::array<int64_t, 2> indices = { 0, 1 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    // unsigned volume of an edge = length / 1! = length
    CHECK(vol == Approx(5.0));
}

TEST_CASE("VolumeTransform 1D edge (signed)", "[quiver_bridge]") {
    // 1D edge: 1 row, 2 cols -> rows+1 == cols -> signed volume = length
    std::vector<std::array<double, 1>> positions = {
        { 0.0 },
        { 5.0 }
    };
    std::array<int64_t, 2> indices = { 0, 1 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    double vol = bgq::VolumeTransform{}(id);

    CHECK(vol == Approx(5.0));
}

TEST_CASE("CircumcenterTransform edge midpoint", "[quiver_bridge]") {
    // Circumcenter of an edge = midpoint
    std::vector<std::array<double, 3>> positions = {
        { 1.0, 2.0, 3.0 },
        { 5.0, 6.0, 7.0 }
    };
    std::array<int64_t, 2> indices = { 0, 1 };

    auto id = bgq::make_incident_data(positions.data(), indices);
    auto C = bgq::CircumcenterTransform{}(id);

    CHECK(C[0] == Approx(3.0));
    CHECK(C[1] == Approx(4.0));
    CHECK(C[2] == Approx(5.0));
}
