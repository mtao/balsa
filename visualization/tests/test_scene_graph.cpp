#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <balsa/scene_graph/Camera.hpp>
#include <balsa/scene_graph/ImageData.hpp>
#include <balsa/scene_graph/BasicShading.hpp>
#include <balsa/scene_graph/MeshGeometry.hpp>
#include <balsa/scene_graph/MeshInstance.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/visualization/mesh_conversion.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/types.hpp>
#include <balsa/visualization/image_io.hpp>


#include <zipper/storage/layout_types.hpp>
#include <zipper/transform/common.hpp>
#include <zipper/transform/model.hpp>
#include <zipper/transform/quaternion_transform.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

// ── Helpers: fill MeshGeometry with plain buffers ───────────────────

// Test-local mdspan shorthands (MeshGeometry accepts any rank-2 mdspan).
using DynIdx = ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>;
static auto TriangleIndices(const uint32_t *p, std::size_t n) -> DynIdx {
    return DynIdx(p, n, 3);
}
static auto EdgeIndices(const uint32_t *p, std::size_t n) -> DynIdx {
    return DynIdx(p, n, 2);
}

// Two-triangle quad (vertices 0-3), optionally with normals.
static auto fill_quad(balsa::scene_graph::MeshGeometry &md,
                      bool with_normals = false) -> void {
    using balsa::scene_graph::AttributeDomain;
    const std::array<float, 12> pos = {
        0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    md.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos);
    if (with_normals) {
        const std::array<float, 12> nrm = {
            0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
        md.add_attribute("vertex_normals", AttributeDomain::Vertex, 3, nrm);
    }
    const std::array<uint32_t, 6> tris = {0, 1, 2, 0, 2, 3};
    md.set_triangle_indices(TriangleIndices(tris.data(), tris.size() / 3));
}

static auto fill_single_tri(balsa::scene_graph::MeshGeometry &md) -> void {
    using balsa::scene_graph::AttributeDomain;
    const std::array<double, 9> pos = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    md.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos);
    const std::array<uint32_t, 3> tris = {0, 1, 2};
    md.set_triangle_indices(TriangleIndices(tris.data(), tris.size() / 3));
}

// Element-major values of an attribute stored as T (empty if not T).
template <typename T>
static auto flat_values(const balsa::scene_graph::MeshAttribute &a)
    -> std::vector<T> {
    const auto v = a.view<T>();
    if (!v) return {};
    return {v->data_handle(), v->data_handle() + v->size()};
}

// Element-major contents of a K x count index view.
template <typename View>
static auto flat_indices(const View &v) -> std::vector<uint32_t> {
    return {v.data_handle(), v.data_handle() + v.size()};
}

// An Object showing its own geometry with the built-in shading.
struct MeshObject {
    balsa::scene_graph::Object object{"mesh"};
    balsa::scene_graph::MeshInstance &instance =
        object.emplace_feature<balsa::scene_graph::MeshInstance>(
            std::make_shared<balsa::scene_graph::MeshGeometry>());
    balsa::scene_graph::MeshGeometry &geometry = *instance.geometry();
    balsa::scene_graph::BasicShading &shading =
        object.emplace_feature<balsa::scene_graph::BasicShading>();
};

TEST_CASE("Object creation and hierarchy", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object root("root");
    CHECK(root.name == "root");
    CHECK(root.parent() == nullptr);
    CHECK(root.children_count() == 0);

    auto &child1 = root.add_child("child1");
    auto &child2 = root.add_child("child2");

    CHECK(root.children_count() == 2);
    CHECK(child1.parent() == &root);
    CHECK(child2.parent() == &root);
    CHECK(child1.name == "child1");
    CHECK(child2.name == "child2");
}

TEST_CASE("Object features", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object obj("test_obj");

    // Attach a Camera feature.
    auto &cam = obj.emplace_feature<Camera>();
    cam.set_perspective(0.7854f, 1.0f, 0.01f, 100.0f);
    CHECK(cam.projection_type() == Camera::Projection::Perspective);
    CHECK(cam.fov_y() == Catch::Approx(0.7854f));

    // Find the camera feature.
    Camera *found = obj.find_feature<Camera>();
    REQUIRE(found != nullptr);
    CHECK(found == &cam);

    // Find a feature that doesn't exist returns nullptr.
    MeshInstance *mesh = obj.find_feature<MeshInstance>();
    CHECK(mesh == nullptr);
}

TEST_CASE("Object transform hierarchy", "[scene_graph]") {
    using namespace balsa::scene_graph;

    // Root has identity transform by default.
    Object root("root");

    // The world_transform of a root node should be identity.
    // We can't easily compare zipper matrices element-wise here,
    // but we can check that root.parent() is null.
    CHECK(root.parent() == nullptr);

    // Add child and verify it's connected.
    auto &child = root.add_child("child");
    CHECK(child.parent() == &root);
}

TEST_CASE("MeshGeometry and BasicShading version tracking", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &geo = m.geometry;
    auto &sh = m.shading;

    // Initially empty
    CHECK(sh.vertex_count() == 0);
    CHECK(geo.triangle_count() == 0);
    CHECK(geo.edge_count() == 0);
    CHECK_FALSE(sh.has_positions());
    CHECK_FALSE(sh.has_normals());
    CHECK_FALSE(geo.has_triangle_indices());
    CHECK_FALSE(geo.has_edge_indices());
    CHECK_FALSE(sh.has_scalar_field());

    const uint64_t g0 = geo.version();
    const uint64_t s0 = sh.version();

    // Geometry changes bump only the geometry version; the shading picks
    // up the conventionally named attributes on its own.
    fill_quad(geo, /*with_normals=*/true);
    CHECK(geo.version() > g0);
    CHECK(sh.version() == s0);
    CHECK(sh.vertex_count() == 4);
    CHECK(sh.has_positions());
    CHECK(sh.has_normals());
    CHECK(geo.has_triangle_indices());
    CHECK(geo.triangle_count() == 2);
    CHECK(geo.has_edge_indices());

    // Role changes bump only the shading version.
    const uint64_t g1 = geo.version();
    sh.clear_normal();
    CHECK(sh.version() > s0);
    CHECK(geo.version() == g1);
}

TEST_CASE("MeshGeometry auto-derives edges from triangles", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo;

    // Two triangles sharing an edge:
    //   tri 0: (0, 1, 2)
    //   tri 1: (0, 2, 3)
    //
    // Expected unique edges: 0-1, 1-2, 0-2, 2-3, 0-3 = 5 edges
    fill_quad(geo);

    CHECK(geo.has_triangle_indices());
    CHECK(geo.triangle_count() == 2);

    // Edges should be auto-derived from the triangles.
    CHECK(geo.has_edge_indices());
    CHECK(geo.edge_count() == 5);

    // Verify edge indices are valid vertex references.
    auto edges = geo.edge_indices();
    REQUIRE(edges.extent(1) == 5);
    for (std::size_t e = 0; e < edges.extent(1); ++e) {
        CHECK(edges[0, e] < edges[1, e]); // stored low, high
        CHECK(edges[1, e] < 4);
    }
}

TEST_CASE("MeshGeometry topology from single triangle", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo;
    fill_single_tri(geo);

    CHECK(geo.triangle_count() == 1);
    CHECK(geo.edge_count() == 3); // A single triangle has 3 edges
}

TEST_CASE("MeshGeometry edge-only mesh has no triangles", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    const std::array<float, 9> pos = {0, 0, 0, 1, 0, 0, 2, 0, 0};
    m.geometry.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos);
    const std::array<uint32_t, 4> edges = {0, 1, 1, 2};
    m.geometry.set_edge_indices(EdgeIndices(edges.data(), edges.size() / 2));

    // Edge mesh: has edges but no triangles.
    CHECK_FALSE(m.geometry.has_triangle_indices());
    CHECK(m.geometry.has_edge_indices());
    CHECK(m.geometry.edge_count() == 2);
    CHECK(m.shading.has_positions());
    CHECK(m.shading.vertex_count() == 3);
}

TEST_CASE("BasicShading role assignment and clearing", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &sh = m.shading;
    fill_quad(m.geometry, /*with_normals=*/true);

    // Conventionally named attributes are bound automatically.
    CHECK(sh.has_positions());
    CHECK(sh.has_normals());
    CHECK_FALSE(sh.has_scalar_field());

    uint64_t v1 = sh.version();

    // Clear positions.
    sh.clear_position();
    CHECK_FALSE(sh.has_positions());
    CHECK(sh.version() > v1);

    // Clear normals.
    uint64_t v2 = sh.version();
    sh.clear_normal();
    CHECK_FALSE(sh.has_normals());
    CHECK(sh.version() > v2);

    // A cleared role stays cleared when the geometry changes.
    fill_quad(m.geometry, /*with_normals=*/true);
    CHECK_FALSE(sh.has_positions());
    CHECK_FALSE(sh.has_normals());

    // Re-assign position by name.
    uint64_t v3 = sh.version();
    CHECK(sh.assign_position("vertex_positions"));
    CHECK(sh.has_positions());
    CHECK(sh.version() > v3);
}

TEST_CASE("BasicShading without geometry binds nothing", "[scene_graph]") {
    using namespace balsa::scene_graph;

    BasicShading detached; // not attached to an Object
    CHECK(detached.geometry() == nullptr);
    CHECK_FALSE(detached.has_positions());
    CHECK_FALSE(detached.assign_position("vertex_positions"));

    Object obj("shading only");
    auto &sh = obj.emplace_feature<BasicShading>();
    CHECK(sh.geometry() == nullptr);
    CHECK_FALSE(sh.has_positions());

    // An instance without geometry binds nothing either.
    auto &instance = obj.emplace_feature<MeshInstance>();
    CHECK(sh.geometry() == nullptr);
    CHECK_FALSE(sh.has_positions());

    // Showing geometry later is picked up.
    auto geometry = std::make_shared<MeshGeometry>();
    fill_quad(*geometry);
    instance.set_geometry(geometry);
    CHECK(sh.geometry() == geometry.get());
    CHECK(sh.has_positions());

    // Attributes of other geometry are rejected.
    MeshGeometry other;
    fill_quad(other);
    CHECK_FALSE(sh.assign_position(other.find_attribute("vertex_positions")));
}

TEST_CASE("MeshGeometry attributes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo;
    fill_quad(geo, /*with_normals=*/true);

    // Should have at least vertex_positions and vertex_normals.
    bool found_pos = false;
    bool found_nrm = false;
    for (const auto &da : geo.attributes()) {
        if (da->name == "vertex_positions") {
            found_pos = true;
            CHECK(da->domain == AttributeDomain::Vertex);
            CHECK(da->component_count() == 3);
            CHECK(da->count() == 4);
        }
        if (da->name == "vertex_normals") {
            found_nrm = true;
            CHECK(da->domain == AttributeDomain::Vertex);
            CHECK(da->component_count() == 3);
            CHECK(da->count() == 4);
        }
    }
    CHECK(found_pos);
    CHECK(found_nrm);
}

TEST_CASE("MeshGeometry stores topology as attributes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo;
    CHECK(geo.triangle_indices().extent(1) == 0);
    CHECK(geo.edge_indices().extent(1) == 0);
    fill_quad(geo);

    // Triangles: Face-domain uint32 attribute, 3 x F; edges: Edge-domain
    // 2 x E, derived from the triangles.
    const auto faces = geo.find_attribute(MeshGeometry::face_vertices_name);
    REQUIRE(faces);
    CHECK(faces->domain == AttributeDomain::Face);
    CHECK(faces->type() == AttributeType::UInt32);
    CHECK(faces->component_count() == 3);
    CHECK(faces->count() == 2);
    const auto edges = geo.find_attribute(MeshGeometry::edge_vertices_name);
    REQUIRE(edges);
    CHECK(edges->domain == AttributeDomain::Edge);
    CHECK(edges->component_count() == 2);
    CHECK(edges->count() == geo.edge_count());

    const auto tris = geo.triangle_indices();
    CHECK(tris.static_extent(0) == 3);
    CHECK(tris[2, 1] == 3u); // third corner of the second triangle
    CHECK(tris.data_handle() == faces->view<uint32_t>()->data_handle());

    // Reserved names can't be added or removed directly.
    const std::array<float, 2> junk = {0, 0};
    CHECK_FALSE(geo.add_attribute(".face_vertices", AttributeDomain::Face, 1, junk));
    CHECK_FALSE(geo.add_attribute(".anything", AttributeDomain::Vertex, 1, junk));
    CHECK_FALSE(geo.remove_attribute(MeshGeometry::face_vertices_name));
    CHECK(geo.triangle_count() == 2);

    // Face attributes line up with the face attribute's count.
    const std::array<float, 2> area = {0.5f, 0.5f};
    REQUIRE(geo.add_attribute("face_area", AttributeDomain::Face, 1, area));
    CHECK(geo.find_attribute("face_area")->count() == geo.triangle_count());

    // Topology is copied with the other attributes.
    MeshGeometry copy;
    copy.copy_from(geo);
    CHECK(flat_indices(copy.triangle_indices())
          == flat_indices(geo.triangle_indices()));
    CHECK(copy.edge_count() == geo.edge_count());
}

TEST_CASE("MeshGeometry explicit edges override derived", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo;
    fill_quad(geo);
    CHECK(geo.edge_count() == 5);

    const std::array<uint32_t, 4> edges = {0, 1, 2, 3};
    geo.set_edge_indices(EdgeIndices(edges.data(), edges.size() / 2));
    CHECK(geo.edge_count() == 2);

    // Re-setting triangles keeps the explicit edges.
    const std::array<uint32_t, 3> tri = {0, 1, 2};
    geo.set_triangle_indices(TriangleIndices(tri.data(), tri.size() / 3));
    CHECK(geo.edge_count() == 2);
}

TEST_CASE("BasicShading scalar binding extracts components", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &sh = m.shading;
    fill_quad(m.geometry);
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    m.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv);

    REQUIRE(sh.assign_scalar("uv", 0));
    const auto &b = sh.scalar_binding();
    CHECK(b.component_count() == 1);
    REQUIRE(b.size() == 4);
    auto u = b.view<float, 1>();
    REQUIRE(u);
    CHECK((*u)[0, 0] == 3.0f);
    CHECK((*u)[0, 1] == 0.0f);

    REQUIRE(sh.assign_scalar("uv", -1)); // magnitude
    auto mag = sh.scalar_binding().view<float, 1>();
    REQUIRE(mag);
    CHECK((*mag)[0, 0] == Catch::Approx(5.0f));
    CHECK(sh.scalar_component() == -1);

    CHECK_FALSE(sh.assign_scalar("uv", 2)); // out of range
    CHECK_FALSE(sh.has_scalar_field());
    CHECK_FALSE(sh.assign_scalar("missing"));
}

TEST_CASE("BasicShading follows geometry changes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &geo = m.geometry;
    auto &sh = m.shading;
    fill_quad(geo);
    REQUIRE(sh.has_positions());

    // Adding more attributes may reallocate storage; binding must follow.
    for (int i = 0; i < 16; ++i) {
        const std::array<float, 4> s = {0, 1, 2, 3};
        geo.add_attribute("s" + std::to_string(i), AttributeDomain::Vertex, 1, s);
    }
    auto pos = geo.find_attribute("vertex_positions");
    REQUIRE(pos);
    CHECK(sh.position_binding().attribute() == pos);
    CHECK(sh.position_binding().view<float>()->data_handle()
          == pos->view<float>()->data_handle());

    // Replacing the bound attribute updates the binding.
    const std::array<float, 6> pos2 = {0, 0, 1, 1, 2, 2};
    geo.add_attribute("vertex_positions", AttributeDomain::Vertex, 2, pos2);
    CHECK(sh.vertex_count() == 3);
    CHECK(sh.position_binding().component_count() == 2);

    // Replacing in place keeps handle identity.
    CHECK(sh.position_binding().attribute() == pos);

    // A magnitude binding is recomputed when its attribute is replaced.
    REQUIRE(sh.assign_scalar("vertex_positions", -1));
    CHECK((*sh.scalar_binding().view<float, 1>())[0, 2] == Catch::Approx(std::sqrt(8.0f)));
    const std::array<float, 6> pos3 = {3, 4, 0, 0, 0, 0};
    geo.add_attribute("vertex_positions", AttributeDomain::Vertex, 2, pos3);
    CHECK((*sh.scalar_binding().view<float, 1>())[0, 0] == Catch::Approx(5.0f));

    // Removing it unbinds; a held handle stays readable but detached.
    CHECK(geo.remove_attribute("vertex_positions"));
    CHECK_FALSE(sh.has_positions());
    CHECK_FALSE(sh.has_scalar_field());
    CHECK(pos->count() == 3);
    CHECK_FALSE(sh.assign_position(pos));
}

TEST_CASE("MeshGeometry face attributes are stored but not bindable", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    fill_quad(m.geometry);
    const std::array<float, 2> area = {0.5f, 0.5f};
    m.geometry.add_attribute("face_area", AttributeDomain::Face, 1, area);

    auto a = m.geometry.find_attribute("face_area");
    REQUIRE(a);
    CHECK(a->domain == AttributeDomain::Face);
    CHECK(a->count() == 2);
    CHECK_FALSE(m.shading.assign_scalar("face_area"));
}

TEST_CASE("to_mesh_geometry converts OBJ into MeshGeometry", "[scene_graph]") {
    using namespace balsa::scene_graph;

    auto path = std::filesystem::temp_directory_path() / "balsa_quad.obj";
    {
        std::ofstream f(path);
        f << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3\nf 1 3 4\n";
    }
    auto obj = balsa::geometry::triangle_mesh::read_objF(path);
    std::filesystem::remove(path);

    MeshObject m;
    balsa::visualization::to_mesh_geometry(obj, m.geometry);
    CHECK(m.shading.vertex_count() == 4);
    CHECK(m.geometry.triangle_count() == 2);
    CHECK(m.geometry.edge_count() == 5);
    CHECK_FALSE(m.shading.has_normals());
}

TEST_CASE("MeshGeometry accepts indices in any orientation and layout",
          "[scene_graph]") {
    using namespace balsa::scene_graph;
    namespace zs = ::zipper::storage;
    using ::zipper::dynamic_extent;
    using ::zipper::extents;
    using ::zipper::mdspan;

    // Two triangles (0,1,2), (0,2,3).
    const std::array<uint32_t, 6> expected = {0, 1, 2, 0, 2, 3};

    auto check = [&](MeshGeometry &md) {
        CHECK(std::ranges::equal(flat_indices(md.triangle_indices()), expected));
        CHECK(md.edge_count() == 5);
    };

    SECTION("N x 3 row-major") {
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const uint32_t, extents<dynamic_extent, 3>, zs::layout_right>(
                expected.data(), 2)));
        check(md);
    }
    SECTION("3 x N column-major (same buffer)") {
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const uint32_t, extents<3, dynamic_extent>, zs::layout_left>(
                expected.data(), 2)));
        check(md);
    }
    SECTION("3 x N row-major (vertex-major buffer, transposed)") {
        const std::array<uint32_t, 6> buf = {0, 0, 1, 2, 2, 3};
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const uint32_t, extents<3, dynamic_extent>, zs::layout_right>(
                buf.data(), 2)));
        check(md);
    }
    SECTION("fully dynamic extents, int64 values") {
        const std::array<int64_t, 6> buf = {0, 1, 2, 0, 2, 3};
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const int64_t, ::zipper::dextents<2>>(buf.data(), 2, 3)));
        check(md);
    }
    SECTION("strided column slice of a wider table") {
        // Rows: tri id, v0, v1, v2 -> take columns 1..3 via layout_stride.
        const std::array<uint32_t, 8> table = {7, 0, 1, 2, 8, 0, 2, 3};
        using E = extents<dynamic_extent, 3>;
        zs::layout_stride::mapping<E> map(E(2), std::array<std::size_t, 2>{4, 1});
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const uint32_t, E, zs::layout_stride>(table.data() + 1, map)));
        check(md);
    }
    SECTION("square 3 x 3: rows are triangles") {
        const std::array<uint32_t, 9> buf = {0, 1, 2, 0, 2, 3, 1, 2, 3};
        MeshGeometry md;
        REQUIRE(md.set_triangle_indices(
            mdspan<const uint32_t, extents<3, 3>, zs::layout_right>(buf.data())));
        CHECK(std::ranges::equal(flat_indices(md.triangle_indices()), buf));
    }
    SECTION("wrong shape is rejected") {
        MeshGeometry md;
        CHECK_FALSE(md.set_triangle_indices(
            mdspan<const uint32_t, ::zipper::dextents<2>>(expected.data(), 2, 2)));
        CHECK_FALSE(md.has_triangle_indices());
    }
    SECTION("edges, 2 x N column-major") {
        const std::array<uint32_t, 4> e = {0, 1, 2, 3};
        MeshGeometry md;
        REQUIRE(md.set_edge_indices(
            mdspan<const uint32_t, extents<2, dynamic_extent>, zs::layout_left>(
                e.data(), 2)));
        CHECK(std::ranges::equal(flat_indices(md.edge_indices()), e));
    }
}

TEST_CASE("MeshGeometry and BasicShading copy_from", "[scene_graph]") {
    using namespace balsa::scene_graph;
    using balsa::visualization::vulkan::NormalSource;

    MeshObject src;
    fill_quad(src.geometry, /*with_normals=*/true);
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    src.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv);
    REQUIRE(src.shading.assign_scalar("uv", 1));
    src.shading.clear_normal();
    src.shading.render_state().normal_source = NormalSource::None;

    MeshObject dst;
    dst.geometry.copy_from(src.geometry);
    dst.shading.copy_from(src.shading);

    // Geometry: same contents, independent attribute instances.
    CHECK(dst.geometry.triangle_count() == 2);
    CHECK(dst.geometry.edge_count() == 5);
    const auto src_uv = src.geometry.find_attribute("uv");
    const auto dst_uv = dst.geometry.find_attribute("uv");
    REQUIRE(dst_uv);
    CHECK_FALSE(dst_uv == src_uv);
    CHECK_FALSE(dst.geometry.owns(src_uv));
    CHECK(flat_values<float>(*dst_uv) == flat_values<float>(*src_uv));

    // Shading: roles re-targeted by name; cleared roles stay cleared.
    CHECK(dst.shading.position_binding().attribute()
          == dst.geometry.find_attribute("vertex_positions"));
    CHECK(dst.shading.scalar_binding().attribute() == dst_uv);
    CHECK(dst.shading.scalar_component() == 1);
    CHECK((*dst.shading.scalar_binding().view<float, 1>())[0, 0] == 4.0f);
    CHECK_FALSE(dst.shading.has_normals());
    CHECK(dst.shading.render_state().normal_source == NormalSource::None);

    // Changing the copy leaves the source alone.
    const std::array<float, 8> uv2 = {0, 9, 0, 9, 0, 9, 0, 9};
    dst.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv2);
    CHECK((*dst.shading.scalar_binding().view<float, 1>())[0, 0] == 9.0f);
    CHECK((*src.shading.scalar_binding().view<float, 1>())[0, 0] == 4.0f);
}

TEST_CASE("MeshInstance and BasicShading as Object features", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object obj("mesh_obj");
    auto geo = std::make_shared<MeshGeometry>();
    auto &instance = obj.emplace_feature<MeshInstance>(geo);
    auto &sh = obj.emplace_feature<BasicShading>();

    // Verify they can be found via find_feature.
    CHECK(obj.find_feature<MeshInstance>() == &instance);
    CHECK(obj.find_feature<BasicShading>() == &sh);
    CHECK(sh.geometry() == geo.get());

    // Fill the geometry and verify through the shading.
    fill_single_tri(*geo);
    CHECK(sh.vertex_count() == 3);
    CHECK(sh.has_positions());
}

TEST_CASE("Objects share geometry with their own shading", "[scene_graph]") {
    using namespace balsa::scene_graph;

    auto geo = std::make_shared<MeshGeometry>();
    fill_quad(*geo);
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    geo->add_attribute("uv", AttributeDomain::Vertex, 2, uv);

    Object a("a");
    Object b("b");
    a.emplace_feature<MeshInstance>(geo);
    b.emplace_feature<MeshInstance>(geo);
    auto &sa = a.emplace_feature<BasicShading>();
    auto &sb = b.emplace_feature<BasicShading>();

    // Same attributes, bound independently.
    REQUIRE(sa.assign_scalar("uv", 0));
    REQUIRE(sb.assign_scalar("uv", 1));
    CHECK(sa.scalar_binding().attribute() == sb.scalar_binding().attribute());
    CHECK((*sa.scalar_binding().view<float, 1>())[0, 0] == 3.0f);
    CHECK((*sb.scalar_binding().view<float, 1>())[0, 0] == 4.0f);

    // A geometry change is seen through both objects.
    const std::array<float, 8> uv2 = {7, 8, 0, 1, 1, 0, 0, 0};
    geo->add_attribute("uv", AttributeDomain::Vertex, 2, uv2);
    CHECK((*sa.scalar_binding().view<float, 1>())[0, 0] == 7.0f);
    CHECK((*sb.scalar_binding().view<float, 1>())[0, 0] == 8.0f);

    // The geometry outlives the objects showing it.
    std::weak_ptr<MeshGeometry> weak = geo;
    geo.reset();
    CHECK_FALSE(weak.expired());

    // Showing different geometry rebinds; roles bound by default follow
    // the new geometry's vertex_positions, explicit ones without a match
    // unbind.
    auto other = std::make_shared<MeshGeometry>();
    fill_single_tri(*other);
    a.find_feature<MeshInstance>()->set_geometry(other);
    CHECK(sa.vertex_count() == 3);
    CHECK_FALSE(sa.has_scalar_field());
    CHECK(sb.vertex_count() == 4);
}

TEST_CASE("Object detach", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object root("root");
    auto &child1 = root.add_child("child1");
    root.add_child("child2");
    CHECK(root.children_count() == 2);

    // Detach child1
    auto detached = child1.detach();
    REQUIRE(detached != nullptr);
    CHECK(detached->name == "child1");
    CHECK(detached->parent() == nullptr);
    CHECK(root.children_count() == 1);
    CHECK(root.children()[0]->name == "child2");
}

TEST_CASE("Object visibility flag", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object obj("test");
    CHECK(obj.visible == true);

    obj.visible = false;
    CHECK(obj.visible == false);
}

// ── TRS Transform Tests ────────────────────────────────────────────

TEST_CASE("Object default TRS is identity", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("identity");

    // Translation should be zero.
    CHECK(static_cast<float>(obj.translation()(0)) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.translation()(1)) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.translation()(2)) == Catch::Approx(0.0f));

    // Rotation should be identity quaternion (w=1, x=y=z=0).
    CHECK(static_cast<float>(obj.rotation().w()) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.rotation().x()) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.rotation().y()) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.rotation().z()) == Catch::Approx(0.0f));

    // Scale should be (1, 1, 1).
    CHECK(static_cast<float>(obj.scale_factors()(0)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.scale_factors()(1)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.scale_factors()(2)) == Catch::Approx(1.0f));

    // local_transform() should be identity (4×4 with ones on diagonal).
    auto xf = obj.local_transform();
    auto m = xf.to_matrix();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float expected = (r == c) ? 1.0f : 0.0f;
            CHECK(static_cast<float>(m(r, c))
                  == Catch::Approx(expected).margin(1e-6f));
        }
    }
}

TEST_CASE("Object set/get translation", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("translated");
    Vec3f t;
    t(0) = 1.0f;
    t(1) = 2.0f;
    t(2) = 3.0f;
    obj.set_translation(t);

    CHECK(static_cast<float>(obj.translation()(0)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.translation()(1)) == Catch::Approx(2.0f));
    CHECK(static_cast<float>(obj.translation()(2)) == Catch::Approx(3.0f));

    // local_transform should have translation in last column.
    auto m = obj.local_transform().to_matrix();
    CHECK(static_cast<float>(m(0, 3)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(m(1, 3)) == Catch::Approx(2.0f));
    CHECK(static_cast<float>(m(2, 3)) == Catch::Approx(3.0f));

    // Rotation and scale should still be identity.
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            float expected = (r == c) ? 1.0f : 0.0f;
            CHECK(static_cast<float>(m(r, c))
                  == Catch::Approx(expected).margin(1e-6f));
        }
    }
}

TEST_CASE("Object set/get scale", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("scaled");
    Vec3f s;
    s(0) = 2.0f;
    s(1) = 3.0f;
    s(2) = 0.5f;
    obj.set_scale_factors(s);

    CHECK(static_cast<float>(obj.scale_factors()(0)) == Catch::Approx(2.0f));
    CHECK(static_cast<float>(obj.scale_factors()(1)) == Catch::Approx(3.0f));
    CHECK(static_cast<float>(obj.scale_factors()(2)) == Catch::Approx(0.5f));

    // local_transform should have scale on the diagonal of the 3×3 block.
    auto m = obj.local_transform().to_matrix();
    CHECK(static_cast<float>(m(0, 0)) == Catch::Approx(2.0f));
    CHECK(static_cast<float>(m(1, 1)) == Catch::Approx(3.0f));
    CHECK(static_cast<float>(m(2, 2)) == Catch::Approx(0.5f));

    // Off-diagonal should be zero (no rotation applied).
    CHECK(static_cast<float>(m(0, 1)) == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(m(0, 2)) == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(m(1, 0)) == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("Object set_uniform_scale", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("uniform");
    obj.set_uniform_scale(5.0f);

    CHECK(static_cast<float>(obj.scale_factors()(0)) == Catch::Approx(5.0f));
    CHECK(static_cast<float>(obj.scale_factors()(1)) == Catch::Approx(5.0f));
    CHECK(static_cast<float>(obj.scale_factors()(2)) == Catch::Approx(5.0f));
}

TEST_CASE("Object translate() adds to current translation",
          "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("accum");
    Vec3f d1;
    d1(0) = 1.0f;
    d1(1) = 0.0f;
    d1(2) = 0.0f;
    obj.translate(d1);

    Vec3f d2;
    d2(0) = 0.0f;
    d2(1) = 2.0f;
    d2(2) = 3.0f;
    obj.translate(d2);

    CHECK(static_cast<float>(obj.translation()(0)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.translation()(1)) == Catch::Approx(2.0f));
    CHECK(static_cast<float>(obj.translation()(2)) == Catch::Approx(3.0f));
}

TEST_CASE("Object rotation via quaternion", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("rotated");

    // 90 degrees around Z axis:
    //   quaternion = (cos(45°), 0, 0, sin(45°)) = (√2/2, 0, 0, √2/2)
    float c = std::cos(std::numbers::pi_v<float> / 4.0f);
    float s = std::sin(std::numbers::pi_v<float> / 4.0f);
    Quaternionf q(c, 0.0f, 0.0f, s);
    obj.set_rotation(q);

    CHECK(static_cast<float>(obj.rotation().w()) == Catch::Approx(c));
    CHECK(static_cast<float>(obj.rotation().z()) == Catch::Approx(s));

    // The rotation matrix should map X→Y, Y→-X.
    auto m = obj.local_transform().to_matrix();
    // Column 0 should be (0, 1, 0, 0) (X axis maps to Y).
    CHECK(static_cast<float>(m(0, 0)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 0)) == Catch::Approx(1.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(2, 0)) == Catch::Approx(0.0f).margin(1e-5f));
    // Column 1 should be (-1, 0, 0, 0) (Y axis maps to -X).
    CHECK(static_cast<float>(m(0, 1)) == Catch::Approx(-1.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 1)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(2, 1)) == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("Object euler angle round-trip", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("euler");

    Vec3f euler_deg;
    euler_deg(0) = 30.0f; // pitch (X)
    euler_deg(1) = 45.0f; // yaw (Y)
    euler_deg(2) = 60.0f; // roll (Z)
    obj.set_rotation_euler(euler_deg);

    Vec3f result = obj.rotation_euler();
    CHECK(static_cast<float>(result(0)) == Catch::Approx(30.0f).margin(0.01f));
    CHECK(static_cast<float>(result(1)) == Catch::Approx(45.0f).margin(0.01f));
    CHECK(static_cast<float>(result(2)) == Catch::Approx(60.0f).margin(0.01f));
}

TEST_CASE("Object euler angle zero is identity rotation",
          "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("euler_zero");

    Vec3f zero;
    zero(0) = 0.0f;
    zero(1) = 0.0f;
    zero(2) = 0.0f;
    obj.set_rotation_euler(zero);

    // Should produce identity quaternion.
    CHECK(static_cast<float>(obj.rotation().w())
          == Catch::Approx(1.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().x())
          == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().y())
          == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().z())
          == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("Object reset_transform restores identity", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("reset");

    // Apply non-trivial TRS.
    Vec3f t;
    t(0) = 10.0f;
    t(1) = 20.0f;
    t(2) = 30.0f;
    obj.set_translation(t);

    Vec3f euler;
    euler(0) = 45.0f;
    euler(1) = 30.0f;
    euler(2) = 60.0f;
    obj.set_rotation_euler(euler);
    obj.set_uniform_scale(3.0f);

    // Verify non-identity.
    CHECK(static_cast<float>(obj.translation()(0)) != Catch::Approx(0.0f));

    obj.reset_transform();

    // Translation should be zero.
    CHECK(static_cast<float>(obj.translation()(0)) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.translation()(1)) == Catch::Approx(0.0f));
    CHECK(static_cast<float>(obj.translation()(2)) == Catch::Approx(0.0f));

    // Rotation should be identity.
    CHECK(static_cast<float>(obj.rotation().w()) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.rotation().x()) == Catch::Approx(0.0f));

    // Scale should be (1,1,1).
    CHECK(static_cast<float>(obj.scale_factors()(0)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.scale_factors()(1)) == Catch::Approx(1.0f));
    CHECK(static_cast<float>(obj.scale_factors()(2)) == Catch::Approx(1.0f));
}

TEST_CASE("Object set_from_transform round-trip", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    // Build a known T*R*S transform and verify round-trip through
    // set_from_transform → translation/rotation/scale_factors.
    Object src("source");

    Vec3f t;
    t(0) = 3.0f;
    t(1) = -1.0f;
    t(2) = 7.0f;
    src.set_translation(t);

    Vec3f euler;
    euler(0) = 20.0f;
    euler(1) = 35.0f;
    euler(2) = -10.0f;
    src.set_rotation_euler(euler);

    Vec3f s;
    s(0) = 2.0f;
    s(1) = 0.5f;
    s(2) = 1.5f;
    src.set_scale_factors(s);

    // Get the composed local_transform.
    auto xf = src.local_transform();

    // Decompose it back into a new Object.
    Object dst("dest");
    dst.set_from_transform(xf);

    // Translation should match.
    CHECK(static_cast<float>(dst.translation()(0))
          == Catch::Approx(3.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.translation()(1))
          == Catch::Approx(-1.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.translation()(2))
          == Catch::Approx(7.0f).margin(1e-4f));

    // Scale should match.
    CHECK(static_cast<float>(dst.scale_factors()(0))
          == Catch::Approx(2.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.scale_factors()(1))
          == Catch::Approx(0.5f).margin(1e-4f));
    CHECK(static_cast<float>(dst.scale_factors()(2))
          == Catch::Approx(1.5f).margin(1e-4f));

    // Euler angles should match (check via re-extraction).
    Vec3f dst_euler = dst.rotation_euler();
    CHECK(static_cast<float>(dst_euler(0))
          == Catch::Approx(20.0f).margin(0.1f));
    CHECK(static_cast<float>(dst_euler(1))
          == Catch::Approx(35.0f).margin(0.1f));
    CHECK(static_cast<float>(dst_euler(2))
          == Catch::Approx(-10.0f).margin(0.1f));
}

TEST_CASE("Object local_transform composes T*R*S correctly",
          "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("trs");

    Vec3f t;
    t(0) = 5.0f;
    t(1) = 0.0f;
    t(2) = 0.0f;
    obj.set_translation(t);

    // 90° around Z
    float c = std::cos(std::numbers::pi_v<float> / 4.0f);
    float s = std::sin(std::numbers::pi_v<float> / 4.0f);
    obj.set_rotation(Quaternionf(c, 0.0f, 0.0f, s));

    Vec3f scale;
    scale(0) = 2.0f;
    scale(1) = 2.0f;
    scale(2) = 1.0f;
    obj.set_scale_factors(scale);

    auto m = obj.local_transform().to_matrix();

    // Translation in last column.
    CHECK(static_cast<float>(m(0, 3)) == Catch::Approx(5.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 3)) == Catch::Approx(0.0f).margin(1e-5f));

    // The 3×3 block = R * S.  With 90°-Z rotation and scale (2,2,1):
    //   R = [[0,-1,0],[1,0,0],[0,0,1]]
    //   S = diag(2,2,1)
    //   R*S = [[0,-2,0],[2,0,0],[0,0,1]]
    CHECK(static_cast<float>(m(0, 0)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(0, 1)) == Catch::Approx(-2.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 0)) == Catch::Approx(2.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 1)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(2, 2)) == Catch::Approx(1.0f).margin(1e-5f));
}

TEST_CASE("Object world_transform composes parent chain",
          "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object root("root");
    Vec3f t_root;
    t_root(0) = 10.0f;
    t_root(1) = 0.0f;
    t_root(2) = 0.0f;
    root.set_translation(t_root);

    auto &child = root.add_child("child");
    Vec3f t_child;
    t_child(0) = 0.0f;
    t_child(1) = 5.0f;
    t_child(2) = 0.0f;
    child.set_translation(t_child);

    // Root's world transform = root's local transform (no parent).
    auto root_world = root.world_transform().to_matrix();
    CHECK(static_cast<float>(root_world(0, 3)) == Catch::Approx(10.0f));
    CHECK(static_cast<float>(root_world(1, 3)) == Catch::Approx(0.0f));

    // Child's world transform = root_local * child_local.
    // Since both are pure translations, the result should be (10, 5, 0).
    auto child_world = child.world_transform().to_matrix();
    CHECK(static_cast<float>(child_world(0, 3)) == Catch::Approx(10.0f));
    CHECK(static_cast<float>(child_world(1, 3)) == Catch::Approx(5.0f));
    CHECK(static_cast<float>(child_world(2, 3)) == Catch::Approx(0.0f));
}

TEST_CASE("Object world_transform with rotation propagation",
          "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    // Parent has 90° rotation around Z.
    Object root("root");
    float c = std::cos(std::numbers::pi_v<float> / 4.0f);
    float s = std::sin(std::numbers::pi_v<float> / 4.0f);
    root.set_rotation(Quaternionf(c, 0.0f, 0.0f, s));

    // Child has translation (1, 0, 0) in local space.
    auto &child = root.add_child("child");
    Vec3f t_child;
    t_child(0) = 1.0f;
    t_child(1) = 0.0f;
    t_child(2) = 0.0f;
    child.set_translation(t_child);

    // In world space, the child's translation should be rotated by
    // parent's 90°-Z: (1,0,0) → (0,1,0).
    auto child_world = child.world_transform().to_matrix();
    CHECK(static_cast<float>(child_world(0, 3))
          == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(child_world(1, 3))
          == Catch::Approx(1.0f).margin(1e-5f));
    CHECK(static_cast<float>(child_world(2, 3))
          == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("Object selectability flag", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object obj("selectable_test");
    CHECK(obj.selectable == true);

    obj.selectable = false;
    CHECK(obj.selectable == false);
}

TEST_CASE("Object reparenting via add_child(unique_ptr)", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object parent1("p1");
    Object parent2("p2");

    auto &child = parent1.add_child("orphan");
    CHECK(child.parent() == &parent1);
    CHECK(parent1.children_count() == 1);

    // Detach and reparent.
    auto owned = child.detach();
    REQUIRE(owned != nullptr);
    CHECK(owned->parent() == nullptr);
    CHECK(parent1.children_count() == 0);

    auto &reparented = parent2.add_child(std::move(owned));
    CHECK(reparented.parent() == &parent2);
    CHECK(reparented.name == "orphan");
    CHECK(parent2.children_count() == 1);
}

// ── ImageData Tests ────────────────────────────────────────────────

TEST_CASE("ImageData default state", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    CHECK(img.width() == 0);
    CHECK(img.height() == 0);
    CHECK(img.format() == ImageData::Format::RGBA8);
    CHECK_FALSE(img.has_pixels());
    CHECK(img.pixels().empty());
    CHECK(img.version() == 0);
    CHECK_FALSE(img.dirty_region().has_value());

    // Default display parameters.
    CHECK(img.exposure() == Catch::Approx(0.0f));
    CHECK(img.gamma() == Catch::Approx(2.2f));
    CHECK(img.channel_mode() == ImageData::ChannelMode::RGBA);
}

TEST_CASE("ImageData set_pixels_rgba8", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    uint32_t w = 2, h = 3;
    // 2x3 image, RGBA8 = 4 bytes/pixel = 24 bytes
    std::vector<uint8_t> rgba(w * h * 4, 128);
    // Set a recognizable pixel.
    rgba[0] = 255;
    rgba[1] = 0;
    rgba[2] = 0;
    rgba[3] = 255; // red

    img.set_pixels_rgba8(w, h, rgba);

    CHECK(img.width() == 2);
    CHECK(img.height() == 3);
    CHECK(img.format() == ImageData::Format::RGBA8);
    CHECK(img.has_pixels());
    CHECK(img.bytes_per_pixel() == 4);
    CHECK(img.pixels().size() == 24);
    CHECK(img.version() == 1);

    // Dirty region should cover the full image.
    auto dirty = img.dirty_region();
    REQUIRE(dirty.has_value());
    CHECK(dirty->min(0) == 0);
    CHECK(dirty->min(1) == 0);
    CHECK(dirty->width() == 2);
    CHECK(dirty->height() == 3);
    CHECK(img.is_full_dirty());

    // Verify pixel data was copied.
    auto px = img.pixels();
    CHECK(static_cast<uint8_t>(px[0]) == 255);
    CHECK(static_cast<uint8_t>(px[1]) == 0);
    CHECK(static_cast<uint8_t>(px[2]) == 0);
    CHECK(static_cast<uint8_t>(px[3]) == 255);
}

TEST_CASE("ImageData set_pixels_rgbaf32", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    uint32_t w = 4, h = 2;
    // 4x2 image, RGBAF32 = 16 bytes/pixel = 128 bytes = 32 floats
    std::vector<float> rgba(w * h * 4, 0.5f);
    rgba[0] = 1.0f;
    rgba[1] = 0.0f;
    rgba[2] = 0.0f;
    rgba[3] = 1.0f;

    img.set_pixels_rgbaf32(w, h, rgba);

    CHECK(img.width() == 4);
    CHECK(img.height() == 2);
    CHECK(img.format() == ImageData::Format::RGBAF32);
    CHECK(img.bytes_per_pixel() == 16);
    CHECK(img.pixels().size() == 128);
    CHECK(img.version() == 1);
    CHECK(img.is_full_dirty());
}

TEST_CASE("ImageData version increments on mutations", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    CHECK(img.version() == 0);

    std::vector<uint8_t> rgba(4 * 4 * 4, 200); // 4x4 image
    img.set_pixels_rgba8(4, 4, rgba);
    CHECK(img.version() == 1);

    // Partial update.
    std::vector<uint8_t> patch(2 * 2 * 4, 100); // 2x2 patch
    auto patch_bytes = std::as_bytes(std::span<const uint8_t>(patch));
    img.update_region(1, 1, 2, 2, patch_bytes);
    CHECK(img.version() == 2);

    // Another set_pixels replaces everything.
    img.set_pixels_rgba8(4, 4, rgba);
    CHECK(img.version() == 3);
}

TEST_CASE("ImageData update_region writes correct pixels",
          "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    uint32_t w = 4, h = 4;
    std::vector<uint8_t> rgba(w * h * 4, 0);
    img.set_pixels_rgba8(w, h, rgba);
    img.clear_dirty();

    // Write a 2x2 red patch at (1, 1).
    std::vector<uint8_t> patch(2 * 2 * 4);
    for (size_t i = 0; i < 4; ++i) {
        patch[i * 4 + 0] = 255; // R
        patch[i * 4 + 1] = 0; // G
        patch[i * 4 + 2] = 0; // B
        patch[i * 4 + 3] = 255; // A
    }
    auto patch_bytes = std::as_bytes(std::span<const uint8_t>(patch));
    img.update_region(1, 1, 2, 2, patch_bytes);

    // Verify the dirty region.
    auto dirty = img.dirty_region();
    REQUIRE(dirty.has_value());
    CHECK(dirty->min(0) == 1);
    CHECK(dirty->min(1) == 1);
    CHECK(dirty->width() == 2);
    CHECK(dirty->height() == 2);
    CHECK_FALSE(img.is_full_dirty());

    // Verify pixel at (1, 1) is red.
    auto px = img.pixels();
    size_t offset_1_1 = (1 * w + 1) * 4; // row 1, col 1
    CHECK(static_cast<uint8_t>(px[offset_1_1 + 0]) == 255);
    CHECK(static_cast<uint8_t>(px[offset_1_1 + 1]) == 0);
    CHECK(static_cast<uint8_t>(px[offset_1_1 + 2]) == 0);
    CHECK(static_cast<uint8_t>(px[offset_1_1 + 3]) == 255);

    // Verify pixel at (0, 0) is still black.
    CHECK(static_cast<uint8_t>(px[0]) == 0);
    CHECK(static_cast<uint8_t>(px[1]) == 0);
    CHECK(static_cast<uint8_t>(px[2]) == 0);
    CHECK(static_cast<uint8_t>(px[3]) == 0);
}

TEST_CASE("ImageData update_region merges dirty rectangles",
          "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    uint32_t w = 8, h = 8;
    std::vector<uint8_t> rgba(w * h * 4, 0);
    img.set_pixels_rgba8(w, h, rgba);
    img.clear_dirty();

    // First patch at (0, 0) size 2x2.
    std::vector<uint8_t> p1(2 * 2 * 4, 100);
    img.update_region(0, 0, 2, 2, std::as_bytes(std::span<const uint8_t>(p1)));

    // Second patch at (4, 4) size 3x3.
    std::vector<uint8_t> p2(3 * 3 * 4, 200);
    img.update_region(4, 4, 3, 3, std::as_bytes(std::span<const uint8_t>(p2)));

    // Dirty region should be the union bounding box: (0,0) to (7,7).
    auto dirty = img.dirty_region();
    REQUIRE(dirty.has_value());
    CHECK(dirty->min(0) == 0);
    CHECK(dirty->min(1) == 0);
    CHECK(dirty->width() == 7); // max(0+2, 4+3) - min(0,4) = 7 - 0
    CHECK(dirty->height() == 7);
}

TEST_CASE("ImageData clear_dirty resets tracking", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    std::vector<uint8_t> rgba(4 * 4 * 4, 0);
    img.set_pixels_rgba8(4, 4, rgba);

    REQUIRE(img.dirty_region().has_value());
    REQUIRE(img.is_full_dirty());

    img.clear_dirty();
    CHECK_FALSE(img.dirty_region().has_value());
}

TEST_CASE("ImageData display parameters", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;

    img.set_exposure(2.5f);
    CHECK(img.exposure() == Catch::Approx(2.5f));

    img.set_gamma(1.0f);
    CHECK(img.gamma() == Catch::Approx(1.0f));
    img.set_gamma(0.0f);
    CHECK(img.gamma() == Catch::Approx(1.0f));
    img.set_gamma(std::numeric_limits<float>::infinity());
    CHECK(img.gamma() == Catch::Approx(1.0f));

    img.set_channel_mode(ImageData::ChannelMode::Red);
    CHECK(img.channel_mode() == ImageData::ChannelMode::Red);

    img.set_channel_mode(ImageData::ChannelMode::Luminance);
    CHECK(img.channel_mode() == ImageData::ChannelMode::Luminance);
}

TEST_CASE("ImageData as Object feature", "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    Object obj("image_obj");
    auto &img = obj.emplace_feature<ImageData>();

    ImageData *found = obj.find_feature<ImageData>();
    REQUIRE(found != nullptr);
    CHECK(found == &img);

    std::vector<uint8_t> rgba(2 * 2 * 4, 255);
    img.set_pixels_rgba8(2, 2, rgba);
    CHECK(found->has_pixels());
    CHECK(found->width() == 2);
    CHECK(found->height() == 2);
}

TEST_CASE("ImageData set_pixels throws on insufficient data",
          "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    // 4x4 RGBA8 needs 64 bytes, only provide 10.
    std::vector<uint8_t> too_small(10, 0);
    CHECK_THROWS(img.set_pixels_rgba8(4, 4, too_small));
    CHECK_THROWS(img.set_pixels_rgba8(0, 4, {}));
}

TEST_CASE("ImageData update_region throws on out-of-bounds",
          "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    std::vector<uint8_t> rgba(4 * 4 * 4, 0);
    img.set_pixels_rgba8(4, 4, rgba);

    // Region exceeds image bounds.
    std::vector<uint8_t> patch(3 * 3 * 4, 100);
    CHECK_THROWS(img.update_region(
        2, 2, 3, 3, std::as_bytes(std::span<const uint8_t>(patch))));
    CHECK_THROWS(img.update_region(
        std::numeric_limits<uint32_t>::max(),
        0,
        2,
        1,
        std::as_bytes(std::span<const uint8_t>(patch))));
    CHECK_THROWS(img.update_region(
        0, 0, 0, 1, std::as_bytes(std::span<const uint8_t>(patch))));
}

TEST_CASE("ImageData update_region throws when no image set",
          "[scene_graph][image]") {
    using namespace balsa::scene_graph;

    ImageData img;
    std::vector<uint8_t> patch(4, 100);
    CHECK_THROWS(img.update_region(
        0, 0, 1, 1, std::as_bytes(std::span<const uint8_t>(patch))));
}

// ── PPM I/O Tests ──────────────────────────────────────────────────

TEST_CASE("PPM round-trip save and load", "[image_io]") {
    using namespace balsa::visualization;

    uint32_t w = 3, h = 2;
    // Create a known 3x2 RGBA image.
    std::vector<uint8_t> rgba(w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t i = (y * w + x) * 4;
            rgba[i + 0] = static_cast<uint8_t>(x * 80); // R
            rgba[i + 1] = static_cast<uint8_t>(y * 120); // G
            rgba[i + 2] = static_cast<uint8_t>((x + y) * 40); // B
            rgba[i + 3] = 255; // A (ignored in PPM)
        }
    }

    std::string path = "/tmp/balsa_test_ppm_roundtrip.ppm";
    auto save_result = save_ppm(path, w, h, rgba);
    REQUIRE(save_result.has_value());

    auto load_result = load_ppm(path);
    REQUIRE(load_result.has_value());

    const auto &img = load_result.value();
    CHECK(img.width == w);
    CHECK(img.height == h);
    REQUIRE(img.pixels.size() == w * h * 4);

    // Verify pixel data (RGB should match, alpha should be 255).
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t i = (y * w + x) * 4;
            CHECK(img.pixels[i + 0] == rgba[i + 0]); // R
            CHECK(img.pixels[i + 1] == rgba[i + 1]); // G
            CHECK(img.pixels[i + 2] == rgba[i + 2]); // B
            CHECK(img.pixels[i + 3] == 255); // A
        }
    }

    // Cleanup.
    std::remove(path.c_str());
}

TEST_CASE("PPM loads comments and scales maxval", "[image_io]") {
    using namespace balsa::visualization;

    std::string path = "/tmp/balsa_test_ppm_comments.ppm";
    {
        std::FILE *f = std::fopen(path.c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fprintf(f, "P6\n# dimensions\n2 1\n# range\n100\n");
        const std::array<uint8_t, 6> pixels{0, 50, 100, 100, 0, 50};
        REQUIRE(std::fwrite(pixels.data(), 1, pixels.size(), f)
                == pixels.size());
        std::fclose(f);
    }

    auto result = load_ppm(path);
    REQUIRE(result.has_value());
    CHECK(result->pixels
          == std::vector<uint8_t>{0, 128, 255, 255, 255, 0, 128, 255});
    std::remove(path.c_str());
}

TEST_CASE("PPM save validates input", "[image_io]") {
    using namespace balsa::visualization;

    const std::array<uint8_t, 3> too_small{};
    auto result = save_ppm("/tmp/balsa_test_invalid_save.ppm", 1, 1, too_small);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ImageIOError::InvalidFormat);
}

TEST_CASE("PPM load nonexistent file", "[image_io]") {
    using namespace balsa::visualization;

    auto result = load_ppm("/tmp/balsa_test_nonexistent_file.ppm");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ImageIOError::FileNotFound);
}

TEST_CASE("PPM load invalid format", "[image_io]") {
    using namespace balsa::visualization;

    std::string path = "/tmp/balsa_test_invalid_ppm.ppm";
    // Write something that is not a valid PPM.
    {
        std::FILE *f = std::fopen(path.c_str(), "wb");
        REQUIRE(f != nullptr);
        std::fprintf(f, "NOT_PPM\n");
        std::fclose(f);
    }

    auto result = load_ppm(path);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ImageIOError::InvalidFormat);

    std::remove(path.c_str());
}

TEST_CASE("PPM error_string returns non-empty strings", "[image_io]") {
    using namespace balsa::visualization;

    CHECK(!error_string(ImageIOError::FileNotFound).empty());
    CHECK(!error_string(ImageIOError::InvalidFormat).empty());
    CHECK(!error_string(ImageIOError::ReadError).empty());
    CHECK(!error_string(ImageIOError::WriteError).empty());
}

TEST_CASE("Object rotate() post-multiplies quaternion", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("rotate_accum");

    // Rotate 90° around Z, then another 90° around Z = 180° total.
    float c = std::cos(std::numbers::pi_v<float> / 4.0f);
    float s = std::sin(std::numbers::pi_v<float> / 4.0f);
    Quaternionf q90z(c, 0.0f, 0.0f, s);

    obj.set_rotation(q90z);
    obj.rotate(q90z);

    // The resulting rotation matrix should map X→-X, Y→-Y (180° around Z).
    auto m = obj.local_transform().to_matrix();
    CHECK(static_cast<float>(m(0, 0)) == Catch::Approx(-1.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 1)) == Catch::Approx(-1.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(2, 2)) == Catch::Approx(1.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(0, 1)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(m(1, 0)) == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("role_binding_view", "[scene_graph][mesh]") {
    MeshObject m;
    const std::vector<float> xyz{0, 1, 2, 3, 4, 5};
    m.geometry.add_attribute("vertex_positions",
                             balsa::scene_graph::AttributeDomain::Vertex, 3,
                             std::span<const float>(xyz));
    const auto &b = m.shading.position_binding();
    auto p = b.view<float, 3>();
    REQUIRE(p.has_value());
    CHECK(p->extent(1) == 2);
    CHECK((*p)[1, 0] == 1.0f);
    CHECK((*p)[0, 1] == 3.0f);

    using sg_error = balsa::scene_graph::ViewError;
    const auto wrong_count = b.view<float, 2>();
    REQUIRE_FALSE(wrong_count);
    CHECK(wrong_count.error().reason == sg_error::Reason::WrongComponentCount);
    CHECK(wrong_count.error().actual_components == 3);
    const auto wrong_type = b.view<double>();
    REQUIRE_FALSE(wrong_type);
    CHECK(wrong_type.error().reason == sg_error::Reason::WrongType);
    CHECK(wrong_type.error().actual_type
          == balsa::scene_graph::AttributeType::Float32);
    CHECK(balsa::scene_graph::RoleBinding{}.view<float>().error().reason
          == sg_error::Reason::Unbound);

    auto d = b.view<float>();
    REQUIRE(d);
    CHECK(d->extent(0) == 3);
    CHECK((*d)[2, 1] == 5.0f);
}

TEST_CASE("mesh_attribute_from_mdspan", "[scene_graph][mesh]") {
    namespace sgd = balsa::scene_graph::detail;
    using balsa::scene_graph::AttributeValue;
    STATIC_CHECK(std::is_same_v<sgd::attribute_storage_t<float>, float>);
    STATIC_CHECK(std::is_same_v<sgd::attribute_storage_t<double>, double>);
    STATIC_CHECK(std::is_same_v<sgd::attribute_storage_t<int16_t>, int32_t>);
    STATIC_CHECK(std::is_same_v<sgd::attribute_storage_t<uint8_t>, uint32_t>);
    STATIC_CHECK(AttributeValue<const float>);
    STATIC_CHECK_FALSE(AttributeValue<bool>);
    STATIC_CHECK_FALSE(AttributeValue<int64_t>);
    STATIC_CHECK_FALSE(AttributeValue<long double>);

    namespace sg = balsa::scene_graph;
    using ::zipper::dextents;
    sg::MeshGeometry md;

    // Row-major N x 3 memory viewed as 3 x N column-major: copied as-is.
    const std::vector<float> xyz{0, 1, 2, 3, 4, 5};
    auto p = md.add_attribute(
        "vertex_positions", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<2>, ::zipper::storage::col_major>(
            xyz.data(), 3, 2));
    REQUIRE(p);
    CHECK(p->component_count() == 3);
    CHECK(p->count() == 2);
    CHECK(p->type() == sg::AttributeType::Float32);
    CHECK(flat_values<float>(*p) == xyz);

    // 3 x N row-major doubles: components on rows, kept as double and
    // reordered into element-major storage.
    const std::vector<double> rows{0, 3, 1, 4, 2, 5};
    auto q = md.add_attribute(
        "q", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const double, dextents<2>, ::zipper::storage::row_major>(
            rows.data(), 3, 2));
    REQUIRE(q);
    CHECK(q->type() == sg::AttributeType::Float64);
    CHECK(q->holds<double>());
    CHECK_FALSE(q->view<float>());
    CHECK(flat_values<double>(*q) == std::vector<double>{0, 1, 2, 3, 4, 5});
    auto qv = q->view<double, 3>();
    REQUIRE(qv);
    CHECK((*qv)[1, 1] == 4.0);
    CHECK_FALSE((q->view<double, 2>()));

    // Rank-1 data is a single-component attribute.
    const std::vector<int> s{7, 8, 9};
    auto r = md.add_attribute(
        "s", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const int, dextents<1>>(s.data(), 3));
    REQUIRE(r);
    CHECK(r->component_count() == 1);
    CHECK(r->count() == 3);
    CHECK(r->type() == sg::AttributeType::Int32);
    CHECK(flat_values<int32_t>(*r) == std::vector<int32_t>{7, 8, 9});

    // Narrow integers widen to the 32-bit stored type of the same sign.
    const std::vector<uint8_t> labels{1, 2, 255};
    auto u = md.add_attribute("labels", sg::AttributeDomain::Vertex, 1, labels);
    REQUIRE(u);
    CHECK(u->type() == sg::AttributeType::UInt32);
    CHECK(flat_values<uint32_t>(*u) == std::vector<uint32_t>{1, 2, 255});

    // Zero components is rejected.
    CHECK_FALSE(md.add_attribute(
        "bad", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<2>>(xyz.data(), 0, 2)));
    // Flat data that is not a multiple of the component count is rejected.
    CHECK_FALSE(md.add_attribute("bad", sg::AttributeDomain::Vertex, 4, xyz));

    // Strided input: every other value of an interleaved buffer, i.e. the
    // first 2 of each 4-float record (components stride 1, elements 4).
    const std::vector<double> interleaved{0, 1, 9, 9, 2, 3, 9, 9, 4, 5, 9, 9};
    using Stride = ::zipper::storage::layout_stride;
    auto st = md.add_attribute(
        "uv", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const double, dextents<2>, Stride>(
            interleaved.data(),
            Stride::mapping<dextents<2>>(dextents<2>{2, 3},
                                         std::array<std::size_t, 2>{1, 4})));
    REQUIRE(st);
    CHECK(st->component_count() == 2);
    CHECK(st->count() == 3);
    CHECK(flat_values<double>(*st) == std::vector<double>{0, 1, 2, 3, 4, 5});
}

TEST_CASE("role_binding_keeps_attribute_type", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    MeshObject mesh;
    auto &geo = mesh.geometry;
    auto &md = mesh.shading;

    // Whole-attribute bindings view the attribute itself.
    const std::vector<double> dpos{0.5, 1, 2, 3, 4, 5.25};
    auto d = geo.add_attribute("vertex_positions", sg::AttributeDomain::Vertex, 3, dpos);
    const auto &b = md.position_binding();
    REQUIRE(b.is_bound());
    CHECK(b.type() == sg::AttributeType::Float64);
    CHECK(b.component_count() == 3);
    CHECK(b.size() == 2);
    auto v = b.view<double, 3>();
    REQUIRE(v);
    CHECK(v->data_handle() == d->view<double>()->data_handle());
    CHECK((*v)[0, 0] == 0.5);
    CHECK((*v)[2, 1] == 5.25);
    CHECK_FALSE(b.view<float>());

    // A selected component is a strided view into the attribute.
    const std::vector<uint32_t> pairs{3, 4, 6, 8};
    auto pr = geo.add_attribute("pairs", sg::AttributeDomain::Vertex, 2, pairs);
    REQUIRE(md.assign_scalar("pairs", 1));
    const auto &s = md.scalar_binding();
    CHECK(s.type() == sg::AttributeType::UInt32);
    auto c = s.view<uint32_t, 1>();
    REQUIRE(c);
    CHECK(&(*c)[0, 0] == pr->view<uint32_t>()->data_handle() + 1);
    CHECK((*c)[0, 1] == 8u);

    // Magnitudes of non-float values are double.
    REQUIRE(md.assign_scalar("pairs", -1));
    CHECK(md.scalar_binding().type() == sg::AttributeType::Float64);
    auto m = md.scalar_binding().view<double, 1>();
    REQUIRE(m);
    CHECK((*m)[0, 1] == Catch::Approx(10.0));

    // visit() hands over the bound values in their own type.
    const double sum = md.position_binding().visit([](const auto &values) {
        double total = 0;
        for (std::size_t i = 0; i < values.extent(1); ++i) {
            total += static_cast<double>(values[0, i]);
        }
        return total;
    });
    CHECK(sum == 0.5 + 3.0);
}

TEST_CASE("scalar_binding_magnitude", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    MeshObject mesh;
    auto &md = mesh.shading;
    const std::vector<float> v{3, 4, 0, 0, 2, 0};
    mesh.geometry.add_attribute("v", sg::AttributeDomain::Vertex, 3, v);
    REQUIRE(md.assign_scalar("v", -1));
    auto m = md.scalar_binding().view<float, 1>();
    REQUIRE(m);
    CHECK((*m)[0, 0] == Catch::Approx(5.0f));
    CHECK((*m)[0, 1] == Catch::Approx(2.0f));
    REQUIRE(md.set_scalar_component(1));
    CHECK((*md.scalar_binding().view<float, 1>())[0, 0] == 4.0f);
}

#include <balsa/visualization/normalize_mesh.hpp>
TEST_CASE("normalize_to_unit_box_centers_mesh", "[scene_graph][normalize]") {
    MeshObject m;
    // Box from (10, 20, 30) to (14, 22, 31): center (12, 21, 30.5), range 4.
    // Stored as double: normalization works from any attribute type.
    const std::vector<double> xyz{10, 20, 30, 14, 22, 31};
    m.geometry.add_attribute("vertex_positions",
                             balsa::scene_graph::AttributeDomain::Vertex, 3,
                             std::span<const double>(xyz));
    REQUIRE(balsa::visualization::normalize_to_unit_box(m.object, m.shading));
    auto M = m.object.local_transform().to_matrix();
    auto apply = [&](float x, float y, float z, int r) {
        return M(r, 0) * x + M(r, 1) * y + M(r, 2) * z + M(r, 3);
    };
    // center -> origin; corners -> within [-0.5, 0.5]
    for (int r = 0; r < 3; ++r) CHECK(apply(12, 21, 30.5f, r) == Catch::Approx(0.0f).margin(1e-6));
    CHECK(apply(14, 22, 31, 0) == Catch::Approx(0.5f));
    CHECK(apply(10, 20, 30, 0) == Catch::Approx(-0.5f));
}

#include <balsa/visualization/controls/Panel.hpp>

TEST_CASE("controls_builtin_panels", "[controls]") {
    namespace ctl = balsa::visualization::controls;
    namespace sg = balsa::scene_graph;
    using balsa::visualization::vulkan::NormalSource;
    ctl::register_builtin_panels();
    ctl::register_builtin_panels(); // idempotent
    CHECK(ctl::registered_panels().size() == 8);
    REQUIRE(ctl::registered_tabs().size() == 4);
    CHECK(ctl::registered_tabs()[1]->id == "data");

    ctl::PanelContext empty;
    CHECK(ctl::available_panels(empty, "object").empty());
    CHECK(ctl::available_panels(empty, "data").empty());

    MeshObject mesh;
    auto &obj = mesh.object;
    auto &md = mesh.shading;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    mesh.geometry.add_attribute("vertex_positions", sg::AttributeDomain::Vertex,
                                3, std::span<const float>(xyz));
    ctl::PanelContext ctx{nullptr, &obj};

    auto object_panels = ctl::available_panels(ctx, "object");
    REQUIRE(object_panels.size() == 2);
    CHECK(object_panels[0]->id == "object.object");
    CHECK(ctl::available_panels(ctx, "data").size() == 4);
    CHECK(ctl::available_panels(ctx, "scene").empty()); // no scene

    // get/set round-trip through the Transform panel.
    ctl::Layout transform;
    object_panels[1]->fill_layout(ctx, transform);
    int seen = 0;
    transform.for_each_property([&](const ctl::Property &p) {
        if (const auto *v = std::get_if<ctl::Vec3Input>(&p.widget);
            v && p.label == "Translation") {
            v->set(sg::Vec3f{1.0f, 2.0f, 3.0f});
            CHECK(obj.translation() == sg::Vec3f{1.0f, 2.0f, 3.0f});
            CHECK(v->get() == sg::Vec3f{1.0f, 2.0f, 3.0f});
            ++seen;
        }
    });
    CHECK(seen == 1);

    // Material tab is only available while the mesh is lit.
    CHECK(ctl::available_panels(ctx, "material").size() == 1);
    md.render_state().normal_source = NormalSource::None;
    CHECK(ctl::available_panels(ctx, "material").empty());

    // Shading combo: "From Attribute" disabled without normals; setting a
    // value runs constrain().
    ctl::Layout shading;
    ctl::available_panels(ctx, "data")[1]->fill_layout(ctx, shading);
    shading.for_each_property([&](const ctl::Property &p) {
        const auto *e = std::get_if<ctl::ChoiceInput>(&p.widget);
        if (!e || p.label != "Normals") return;
        CHECK_FALSE(e->option_enabled(0));
        e->set_index(0); // FromAttribute without normals -> downgraded
        CHECK(md.render_state().normal_source == NormalSource::ComputedInShader);
        ++seen;
    });
    CHECK(seen == 2);

    // Attribute binding choice lists attributes and reflects the binding.
    ctl::Layout attrs;
    ctl::available_panels(ctx, "data")[3]->fill_layout(ctx, attrs);
    attrs.for_each_property([&](const ctl::Property &p) {
        const auto *e = std::get_if<ctl::ChoiceInput>(&p.widget);
        if (!e || p.label != "Position") return;
        CHECK(e->options().size() == 2);
        CHECK(e->get_index() == 1);
        e->set_index(0);
        CHECK_FALSE(md.has_positions());
        ++seen;
    });
    CHECK(seen == 3);
}
