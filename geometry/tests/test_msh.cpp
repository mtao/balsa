#include <catch2/catch_all.hpp>

#include <balsa/geometry/io/MshData.hpp>

#include <filesystem>
#include <fstream>

namespace bio = balsa::geometry::io;

namespace {
// Two triangles (quad) on surface entity 1, physical group (2, 7).
constexpr const char *quad_msh = R"($MeshFormat
4.1 0 8
$EndMeshFormat
$PhysicalNames
1
2 7 "plate"
$EndPhysicalNames
$Entities
0 0 1 0
1 0 0 0 1 1 0 1 7 0
$EndEntities
$Nodes
1 4 1 4
2 1 0 4
1
2
3
4
0 0 0
1 0 0
1 1 0
0 1 0
$EndNodes
$Elements
1 2 1 2
2 1 2 2
1 1 2 3
2 1 3 4
$EndElements
)";
} // namespace

TEST_CASE("msh_round_trip", "[geometry][io][msh]") {
    auto dir = std::filesystem::temp_directory_path();
    auto in = dir / "balsa_quad.msh";
    std::ofstream(in) << quad_msh;

    auto res = bio::read_msh(in);
    REQUIRE(res.has_value());
    const auto &msh = *res;

    CHECK(msh.num_node_blocks() == 1);
    CHECK(msh.num_element_blocks() == 1);
    CHECK(msh.max_node_tag() == 4);
    CHECK(msh.max_element_dimension() == 2);
    CHECK(msh.has_element_dimension(2));
    CHECK_FALSE(msh.has_element_dimension(3));

    auto nodes = msh.node_block(0);
    CHECK(nodes.num_nodes() == 4);
    CHECK(nodes.coords_per_node() == 3);
    CHECK(nodes.as_mdspan()[2, 1] == 1.0);

    auto elems = msh.element_block(0);
    CHECK(elems.num_elements() == 2);
    CHECK(elems.stride() == 4);
    CHECK(elems.as_mdspan()[1, 3] == 4);

    CHECK(msh.physical_group_tags(2) == std::vector<int>{7});
    CHECK(msh.element_block_indices_for_physical_group(2, 7)
          == std::vector<size_t>{0});
    REQUIRE(msh.find_physical_group(2, 7) != nullptr);
    CHECK(msh.find_physical_group(2, 7)->name == "plate");

    auto out = dir / "balsa_quad_out.msh";
    REQUIRE(bio::write_msh(out, msh).has_value());
    auto again = bio::read_msh(out);
    REQUIRE(again.has_value());
    CHECK(again->element_block(0).num_elements() == 2);

    std::filesystem::remove(in);
    std::filesystem::remove(out);
}

TEST_CASE("msh_read_errors", "[geometry][io][msh]") {
    CHECK_FALSE(bio::read_msh("/nonexistent/x.msh").has_value());
}
