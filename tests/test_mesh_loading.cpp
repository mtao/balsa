#include <catch2/catch.hpp>
#include <balsa/geometry/polygon_mesh/polygon_buffer_construction.hpp>
#include <vector>
#include <range/v3/view/zip.hpp>
#include <iostream>

TEST_CASE("polygon buffer construction", "[mesh_loading]") {
    std::vector<std::vector<int>> buf;

    buf.emplace_back(std::vector<int>{ 0, 1, 2, 3, 4 });
    buf.emplace_back(std::vector<int>{ 5, 6, 7 });
    buf.emplace_back(std::vector<int>{ 8, 9 });

    auto ret = balsa::geometry::polygon_mesh::from_constainer_container<int>(buf);

    REQUIRE(ret._offsets[0] == 0);
    REQUIRE(ret._offsets[1] == 5);
    REQUIRE(ret._offsets[2] == 8);
    REQUIRE(ret._offsets[3] == 10);


    for (size_t j = 0; j < ret.polygon_count(); ++j) {
        auto p = ret.get_polygon(j);

        const auto &b = buf[j];

        REQUIRE(size_t(p.size()) == b.size());
        for (auto [a, b] : ranges::views::zip(p, b)) {
            CHECK(a == b);
        }
    }
}
