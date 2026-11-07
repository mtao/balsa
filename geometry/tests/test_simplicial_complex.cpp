#include <balsa/geometry/simplicial_complex/boundary.hpp>
#include <balsa/geometry/simplicial_complex/detail/combinations.hpp>
#include <balsa/geometry/simplicial_complex/boundary_matrix.hpp>
#include <balsa/geometry/simplicial_complex/faces.hpp>
#include <balsa/geometry/simplicial_complex/facets.hpp>
#include <catch2/catch_test_macros.hpp>
#include <zipper/CSRMatrix.hpp>
#include <zipper/Vector.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace sc = balsa::geometry::simplicial_complex;

namespace {
template <typename F>
auto columns(const F &faces) {
    std::vector<std::vector<uint32_t>> result;
    for (std::size_t c = 0; c < faces.extent(1); ++c) {
        std::vector<uint32_t> column;
        for (std::size_t r = 0; r < faces.extent(0); ++r) column.push_back(faces(r, c));
        result.push_back(column);
    }
    return result;
}

// Even permutations of `face` are the same oriented simplex; compare a
// canonical rotation (smallest vertex first) for triangles.
auto canonical(std::vector<uint32_t> face) {
    std::ranges::rotate(face, std::ranges::min_element(face));
    return face;
}
} // namespace

TEST_CASE("simplicial_complex_faces", "[geometry][simplicial_complex]") {
    // Two positively oriented tetrahedra sharing face (1, 2, 3).
    balsa::zipper::ColVectors<uint32_t, 4> tets(4, 2);
    const std::array<std::array<uint32_t, 4>, 2> cells{{{0, 1, 2, 3}, {1, 2, 3, 4}}};
    for (std::size_t c = 0; c < 2; ++c) {
        for (std::size_t r = 0; r < 4; ++r) tets(r, c) = cells[c][r];
    }

    CHECK(sc::faces<2>(tets).extent(1) == 9);
    CHECK(sc::faces<3>(tets).extent(1) == 7);
    CHECK(sc::faces<1>(tets).extent(1) == 5);
    CHECK(columns(sc::faces<2>(tets)).front() == std::vector<uint32_t>{0, 1});

    // Boundary: the 6 unshared faces, outward.
    auto boundary = columns(sc::boundary(tets));
    for (auto &face : boundary) face = canonical(face);
    std::ranges::sort(boundary);
    CHECK(boundary
          == std::vector<std::vector<uint32_t>>{
              {0, 1, 3}, {0, 2, 1}, {0, 3, 2}, {1, 2, 4}, {1, 4, 3}, {2, 3, 4}});
}

TEST_CASE("simplicial_complex_boundary_of_triangles", "[geometry][simplicial_complex]") {
    // A quad as triangles (0, 1, 2), (0, 2, 3): border edges follow the
    // winding 0 -> 1 -> 2 -> 3 -> 0.
    balsa::zipper::ColVectors<uint32_t, 3> tris(3, 2);
    const std::array<std::array<uint32_t, 3>, 2> cells{{{0, 1, 2}, {0, 2, 3}}};
    for (std::size_t c = 0; c < 2; ++c) {
        for (std::size_t r = 0; r < 3; ++r) tris(r, c) = cells[c][r];
    }
    auto border = columns(sc::boundary(tris));
    std::ranges::sort(border);
    CHECK(border
          == std::vector<std::vector<uint32_t>>{{0, 1}, {1, 2}, {2, 3}, {3, 0}});
    CHECK(sc::faces<2>(tris).extent(1) == 5);

    // A closed surface has no boundary.
    balsa::zipper::ColVectors<uint32_t, 3> tetrahedron(3, 4);
    const std::array<std::array<uint32_t, 3>, 4> surface{
        {{1, 2, 3}, {0, 3, 2}, {0, 1, 3}, {0, 2, 1}}};
    for (std::size_t c = 0; c < 4; ++c) {
        for (std::size_t r = 0; r < 3; ++r) tetrahedron(r, c) = surface[c][r];
    }
    CHECK(sc::boundary(tetrahedron).extent(1) == 0);
}

TEST_CASE("simplicial_complex_boundary_operator", "[geometry][simplicial_complex]") {
    // Two positively oriented tetrahedra sharing face (1, 2, 3).
    balsa::zipper::ColVectors<uint32_t, 4> tets(4, 2);
    const std::array<std::array<uint32_t, 4>, 2> cells{{{0, 1, 2, 3}, {1, 2, 3, 4}}};
    for (std::size_t c = 0; c < 2; ++c) {
        for (std::size_t r = 0; r < 4; ++r) tets(r, c) = cells[c][r];
    }

    const auto triangles = sc::facets(tets);
    const auto edges = sc::facets(triangles);
    CHECK(triangles.extent(1) == 7);
    CHECK(edges.extent(1) == 9);

    const auto d3 = sc::boundary_matrix(tets, triangles).to_csr();
    const auto d2 = sc::boundary_matrix(triangles, edges).to_csr();
    CHECK(d3.extent(0) == 7);
    CHECK(d3.extent(1) == 2);
    CHECK(d2.extent(0) == 9);
    CHECK(d2.extent(1) == 7);

    // The boundary of a boundary is empty.
    for (std::size_t c = 0; c < 2; ++c) {
        ::zipper::VectorX<double> cell(2);
        cell(0) = c == 0 ? 1.0 : 0.0;
        cell(1) = c == 1 ? 1.0 : 0.0;
        const ::zipper::VectorX<double> faces_of_cell = d3 * cell;
        const ::zipper::VectorX<double> zero = d2 * faces_of_cell;
        for (std::size_t e = 0; e < zero.extent(0); ++e) CHECK(zero(e) == 0.0);
    }

    // The boundary of the whole complex is d3 applied to the sum of cells:
    // the shared face cancels, the rest keep their orientation.
    ::zipper::VectorX<double> all(2);
    all(0) = 1.0;
    all(1) = 1.0;
    const ::zipper::VectorX<double> chain = d3 * all;
    auto from_operator = std::vector<std::vector<uint32_t>>{};
    for (std::size_t f = 0; f < chain.extent(0); ++f) {
        if (chain(f) == 0.0) continue;
        std::vector<uint32_t> face{triangles(0, f), triangles(1, f), triangles(2, f)};
        if (chain(f) < 0.0) std::swap(face[0], face[1]);
        from_operator.push_back(canonical(face));
    }
    std::ranges::sort(from_operator);
    auto from_boundary = columns(sc::boundary(tets));
    for (auto &face : from_boundary) face = canonical(face);
    std::ranges::sort(from_boundary);
    CHECK(from_operator == from_boundary);
    CHECK(from_boundary.size() == 6);
}

TEST_CASE("simplicial_complex_combinations", "[geometry][simplicial_complex]") {
    namespace detail = balsa::geometry::simplicial_complex::detail;
    using Pair = std::array<std::size_t, 2>;
    using Triple = std::array<std::size_t, 3>;
    STATIC_CHECK(detail::binomial(4, 2) == 6);
    STATIC_CHECK(detail::binomial(3, 4) == 0);
    STATIC_CHECK(detail::combinations<4, 2>()[0] == Pair{2, 3});
    STATIC_CHECK(detail::combinations<4, 2>()[5] == Pair{0, 1});
    STATIC_CHECK(detail::combinations<3, 3>()[0] == Triple{0, 1, 2});
    // Facets opposite each vertex, the other vertices in order.
    STATIC_CHECK(detail::opposite_facets<4>()[0] == Triple{1, 2, 3});
    STATIC_CHECK(detail::opposite_facets<4>()[2] == Triple{0, 1, 3});
    STATIC_CHECK(detail::opposite_facets<3>()[1] == Pair{0, 2});
    // Every subset once, each increasing.
    STATIC_CHECK([] {
        auto all = detail::combinations<5, 3>();
        if (!std::ranges::all_of(all, std::ranges::is_sorted)) return false;
        std::ranges::sort(all);
        return std::ranges::adjacent_find(all) == all.end();
    }());
}
