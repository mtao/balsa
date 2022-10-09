#include <catch2/catch_all.hpp>
#include <balsa/data_structures/container_of_containers_to_stacked_contiguous_buffer.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/polygon_mesh/read_obj.hpp>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/iota.hpp>
#include <iostream>
#include <filesystem>

const static std::filesystem::path test_dir = BALSA_TEST_ASSET_PATH;
// This test is somewaht silly, just making sure that the parameter wasn't set to something stupid
TEST_CASE("test assets exist", "[asset path exists]") {
    REQUIRE(std::filesystem::exists(test_dir));
    REQUIRE(std::filesystem::exists(test_dir / "plane.obj"));
    REQUIRE(std::filesystem::exists(test_dir / "plane_uv.obj"));
    REQUIRE(std::filesystem::exists(test_dir / "plane_n.obj"));
    REQUIRE(std::filesystem::exists(test_dir / "plane_uvn.obj"));
    REQUIRE(std::filesystem::exists(test_dir / "plane_poly.obj"));
}

TEST_CASE("polygon buffer construction", "[mesh_loading]") {
    std::vector<std::vector<int>> buf;

    buf.emplace_back(std::vector<int>{ 0, 1, 2, 3, 4 });
    buf.emplace_back(std::vector<int>{ 5, 6, 7 });
    buf.emplace_back(std::vector<int>{ 8, 9 });

    auto ret = balsa::data_structures::container_of_containers_to_stacked_contiguous_buffer<int>(buf);

    REQUIRE(ret._offsets[0] == 0);
    REQUIRE(ret._offsets[1] == 5);
    REQUIRE(ret._offsets[2] == 8);
    REQUIRE(ret._offsets[3] == 10);


    for (size_t j = 0; j < ret.span_count(); ++j) {
        auto p = ret.get_span(j);

        const auto &b = buf[j];

        REQUIRE(size_t(p.size()) == b.size());
        for (auto [a, b] : ranges::views::zip(p, b)) {
            CHECK(a == b);
        }
    }
}

TEST_CASE("load polynomial obj", "[load_poly_plane_obj]") {
    using namespace balsa::geometry::polygon_mesh;
    auto m = read_objF(test_dir / "plane_poly.obj");
    auto m2 = read_objF2(test_dir / "plane_poly.obj");

    const auto &pm = m.position;
    REQUIRE(pm.polygons.polygon_count() == 1);
    auto p = pm.polygons.get_polygon(0);
    std::cout << p << std::endl;
    REQUIRE(p.size() == 6);
    CHECK(p(0) == 0);
    CHECK(p(1) == 1);
    CHECK(p(2) == 3);
    CHECK(p(3) == 5);
    CHECK(p(4) == 4);
    CHECK(p(5) == 2);

    REQUIRE(pm.curves.curve_count() == 1);
    auto l = pm.curves.get_curve(0);
    std::cout << l << std::endl;
    REQUIRE(l.size() == 6);
    CHECK(l(0) == 0);
    CHECK(l(1) == 1);
    CHECK(l(2) == 3);
    CHECK(l(3) == 5);
    CHECK(l(4) == 4);
    CHECK(l(5) == 2);


    REQUIRE(m.position.polygons == m2.position.polygons);
    REQUIRE(m.position.vertices.topRows<2>() == m2.position.vertices);
    REQUIRE((m.position.vertices.row(2).array() == 0).all());
}

TEST_CASE("load triangle objs", "[load_tri_plane_obj]") {
    using namespace balsa::geometry::triangle_mesh;
    spdlog::info("uvn");
    auto uvn = read_objF(test_dir / "plane_uvn.obj");
    spdlog::info("plain");
    auto plain = read_objF(test_dir / "plane.obj");
    spdlog::info("uv");
    auto uv = read_objF(test_dir / "plane_uv.obj");
    spdlog::info("n");
    auto n = read_objF(test_dir / "plane_n.obj");

    CHECK(uvn.position.triangles == plain.position.triangles);

    REQUIRE(uvn.position.vertices.cols() == plain.position.vertices.cols());
    REQUIRE(uvn.position.vertices.cols() == uv.position.vertices.cols());
    REQUIRE(uvn.texture.vertices.cols() == uv.texture.vertices.cols());
    REQUIRE(uvn.normal.vertices.cols() == n.normal.vertices.cols());
    CHECK(uvn.position.vertices == plain.position.vertices);
    CHECK(uvn.position.triangles == uv.position.triangles);
    CHECK(uvn.position.vertices == uv.position.vertices);
    CHECK(uvn.position.triangles == n.position.triangles);
    CHECK(uvn.position.vertices == n.position.vertices);

    CHECK(uvn.texture.triangles == uv.texture.triangles);
    CHECK(uvn.texture.vertices == uv.texture.vertices);

    CHECK(uvn.normal.triangles == n.normal.triangles);
    CHECK(uvn.normal.vertices == n.normal.vertices);


    {
        const auto &E = plain.position.edges;
        REQUIRE(E.cols() == 6);
        CHECK(E(0, 0) == 0);
        CHECK(E(1, 0) == 1);

        CHECK(E(0, 1) == 0);
        CHECK(E(1, 1) == 2);
        CHECK(E(0, 2) == 2);
        CHECK(E(1, 2) == 4);

        CHECK(E(0, 3) == 1);
        CHECK(E(1, 3) == 3);
        CHECK(E(0, 4) == 3);
        CHECK(E(1, 4) == 5);

        CHECK(E(0, 5) == 4);
        CHECK(E(1, 5) == 5);
    }
}
