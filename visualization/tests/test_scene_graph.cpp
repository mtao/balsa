#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <balsa/scene_graph/AttributeSemantic.hpp>
#include <balsa/scene_graph/Camera.hpp>
#include <balsa/scene_graph/GeometryView.hpp>
#include <balsa/scene_graph/ImageData.hpp>
#include <balsa/scene_graph/BasicShading.hpp>
#include <balsa/scene_graph/MeshGeometry.hpp>
#include <balsa/scene_graph/MeshInstance.hpp>
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/visualization/mesh_conversion.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/scene_graph/MeshRenderer.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/types.hpp>
#include <balsa/visualization/image_io.hpp>


#include <zipper/Vector.hpp>
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
#include <stdfloat>
#include <string>
#include <utility>

// ── Helpers: fill MeshGeometry with plain buffers ───────────────────

// Tags for positions / normals, which renderer roles find by semantic.
static const balsa::scene_graph::AttributeOptions as_positions{
    .semantic = balsa::scene_graph::semantics::position()};
static const balsa::scene_graph::AttributeOptions as_normals{
    .semantic = balsa::scene_graph::semantics::normal()};

// Test-local mdspan shorthands (MeshGeometry accepts any rank-2 mdspan).
using DynIdx = ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>;
static auto TriangleIndices(const uint32_t *p, std::size_t n) -> DynIdx {
    return DynIdx(p, n, 3);
}
static auto EdgeIndices(const uint32_t *p, std::size_t n) -> DynIdx {
    return DynIdx(p, n, 2);
}

// The default surface layer of a shading, and what its roles resolve to.
static auto surface_id(const balsa::scene_graph::BasicShading &sh)
    -> balsa::scene_graph::MeshLayerId {
    return sh.find_layer("balsa.phong_surface")->id;
}

static auto bound(const balsa::scene_graph::BasicShading &sh, balsa::scene_graph::MeshRole role)
    -> balsa::scene_graph::MeshAttributeHandle {
    const auto *layer = sh.find_layer("balsa.phong_surface");
    return layer ? sh.layer_attribute(*layer, role)
                 : balsa::scene_graph::MeshAttributeHandle{};
}

// The edges view of a copy of `g` (see GeometryView.hpp).
static auto edges_of(const balsa::scene_graph::MeshGeometry &g)
    -> std::shared_ptr<const balsa::scene_graph::MeshGeometry> {
    const balsa::scene_graph::MeshInstance instance(
        std::make_shared<balsa::scene_graph::MeshGeometry>(g.clone()));
    return instance.view(balsa::scene_graph::views::edges());
}

// Layers drawing the geometry as is, for binding tests without topology.
static auto on_source_view(balsa::scene_graph::BasicShading &sh) -> void {
    for (auto &layer : sh.render_layers()) layer.view = balsa::scene_graph::views::source();
}

static auto vertex_count(const balsa::scene_graph::BasicShading &sh) -> std::size_t {
    const auto positions = bound(sh, balsa::scene_graph::roles::position());
    return positions ? positions->count() : 0;
}

// Two-triangle quad (vertices 0-3), optionally with normals.
static auto make_quad(bool with_normals = false)
    -> balsa::scene_graph::MeshGeometry {
    using balsa::scene_graph::AttributeDomain;
    balsa::scene_graph::MeshGeometry md;
    const std::array<float, 12> pos = {
        0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    md.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos, as_positions);
    if (with_normals) {
        const std::array<float, 12> nrm = {
            0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
        md.add_attribute("vertex_normals", AttributeDomain::Vertex, 3, nrm, as_normals);
    }
    const std::array<uint32_t, 6> tris = {0, 1, 2, 0, 2, 3};
    md.set_triangle_indices(TriangleIndices(tris.data(), tris.size() / 3));
    return md;
}

static auto make_single_tri() -> balsa::scene_graph::MeshGeometry {
    using balsa::scene_graph::AttributeDomain;
    balsa::scene_graph::MeshGeometry md;
    const std::array<double, 9> pos = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    md.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos, as_positions);
    const std::array<uint32_t, 3> tris = {0, 1, 2};
    md.set_triangle_indices(TriangleIndices(tris.data(), tris.size() / 3));
    return md;
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
    explicit MeshObject(balsa::scene_graph::MeshGeometry g = {})
      : instance(object.emplace_feature<balsa::scene_graph::MeshInstance>(
            std::make_shared<balsa::scene_graph::MeshGeometry>(std::move(g)))),
        geometry(*instance.geometry()),
        shading(object.emplace_feature<balsa::scene_graph::BasicShading>()) {}

    balsa::scene_graph::Object object{"mesh"};
    balsa::scene_graph::MeshInstance &instance;
    balsa::scene_graph::MeshGeometry &geometry;
    balsa::scene_graph::BasicShading &shading;
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
    CHECK(vertex_count(sh) == 0);
    CHECK(geo.triangle_count() == 0);
    CHECK(geo.edge_count() == 0);
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::normal()).valid());
    CHECK_FALSE(geo.has_triangle_indices());
    CHECK_FALSE(geo.has_edge_indices());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::scalar()).valid());

    const uint64_t g0 = geo.version();
    const uint64_t s0 = sh.version();

    // Geometry changes bump only the geometry version; the shading picks
    // up the conventionally named attributes on its own.
    geo = make_quad(/*with_normals=*/true);
    CHECK(geo.version() > g0);
    CHECK(sh.version() == s0);
    CHECK(vertex_count(sh) == 4);
    CHECK(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK(bound(sh, balsa::scene_graph::roles::normal()).valid());
    CHECK(geo.has_triangle_indices());
    CHECK(geo.triangle_count() == 2);
    CHECK(edges_of(geo)->has_edge_indices());

    // Replacing the geometry is a change even if the replacement has a
    // lower version of its own.
    const uint64_t before = geo.version();
    geo = MeshGeometry{};
    CHECK(geo.version() > before);
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());
    geo = make_quad(/*with_normals=*/true);

    // Role changes bump only the shading version.
    const uint64_t g1 = geo.version();
    sh.clear_layer_role(surface_id(sh), balsa::scene_graph::roles::normal());
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
    geo = make_quad();

    CHECK(geo.has_triangle_indices());
    CHECK(geo.triangle_count() == 2);

    // Only triangles are stored; the edges view derives the edges.
    CHECK_FALSE(geo.has_edge_indices());
    const auto derived = edges_of(geo);
    CHECK(derived->edge_count() == 5);

    // Verify edge indices are valid vertex references.
    auto edges = derived->edge_indices();
    REQUIRE(edges.extent(1) == 5);
    for (std::size_t e = 0; e < edges.extent(1); ++e) {
        CHECK(edges[0, e] < edges[1, e]); // stored low, high
        CHECK(edges[1, e] < 4);
    }
}

TEST_CASE("MeshGeometry topology from single triangle", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo = make_single_tri();

    CHECK(geo.triangle_count() == 1);
    CHECK(edges_of(geo)->edge_count() == 3); // A single triangle has 3 edges
}

TEST_CASE("MeshGeometry edge-only mesh has no triangles", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    const std::array<float, 9> pos = {0, 0, 0, 1, 0, 0, 2, 0, 0};
    m.geometry.add_attribute("vertex_positions", AttributeDomain::Vertex, 3, pos, as_positions);
    const std::array<uint32_t, 4> edges = {0, 1, 1, 2};
    m.geometry.set_edge_indices(EdgeIndices(edges.data(), edges.size() / 2));
    on_source_view(m.shading);

    // Edge mesh: has edges but no triangles.
    CHECK_FALSE(m.geometry.has_triangle_indices());
    CHECK(m.geometry.has_edge_indices());
    CHECK(m.geometry.edge_count() == 2);
    CHECK(bound(m.shading, balsa::scene_graph::roles::position()).valid());
    CHECK(vertex_count(m.shading) == 3);
}

TEST_CASE("BasicShading role assignment and clearing", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &sh = m.shading;
    m.geometry = make_quad(/*with_normals=*/true);

    // Conventionally named attributes are bound automatically.
    CHECK(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK(bound(sh, balsa::scene_graph::roles::normal()).valid());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::scalar()).valid());

    uint64_t v1 = sh.version();

    // Clear positions.
    sh.clear_layer_role(surface_id(sh), balsa::scene_graph::roles::position());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK(sh.version() > v1);

    // Clear normals.
    uint64_t v2 = sh.version();
    sh.clear_layer_role(surface_id(sh), balsa::scene_graph::roles::normal());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::normal()).valid());
    CHECK(sh.version() > v2);

    // A cleared role stays cleared when the geometry changes.
    m.geometry = make_quad(/*with_normals=*/true);
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::normal()).valid());

    // Re-assign position by name.
    uint64_t v3 = sh.version();
    CHECK(sh.assign_layer_role(surface_id(sh), balsa::scene_graph::roles::position(), "vertex_positions"));
    CHECK(bound(sh, balsa::scene_graph::roles::position()).valid());
    CHECK(sh.version() > v3);
}

TEST_CASE("BasicShading without geometry binds nothing", "[scene_graph]") {
    using namespace balsa::scene_graph;

    BasicShading detached; // not attached to an Object
    CHECK(detached.geometry() == nullptr);
    CHECK_FALSE(bound(detached, balsa::scene_graph::roles::position()).valid());
    CHECK_FALSE(detached.assign_layer_role(surface_id(detached), balsa::scene_graph::roles::position(), "vertex_positions"));

    Object obj("shading only");
    auto &sh = obj.emplace_feature<BasicShading>();
    CHECK(sh.geometry() == nullptr);
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());

    // An instance without geometry binds nothing either.
    auto &instance = obj.emplace_feature<MeshInstance>();
    CHECK(sh.geometry() == nullptr);
    CHECK_FALSE(bound(sh, balsa::scene_graph::roles::position()).valid());

    // Showing geometry later is picked up.
    auto geometry = std::make_shared<MeshGeometry>();
    *geometry = make_quad();
    instance.set_geometry(geometry);
    CHECK(sh.geometry() == geometry.get());
    CHECK(bound(sh, balsa::scene_graph::roles::position()).valid());

    // Attributes of other geometry are rejected.
    MeshGeometry other = make_quad();
    CHECK_FALSE(sh.assign_layer_role(surface_id(sh), balsa::scene_graph::roles::position(), other.find_attribute("vertex_positions")));
}

TEST_CASE("MeshGeometry attributes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo = make_quad(/*with_normals=*/true);

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
    geo = make_quad();

    // Triangles: Face-domain uint32 attribute, 3 x F; edges (Edge-domain,
    // 2 x E) are not stored unless set.
    const auto faces = geo.find_attribute(MeshGeometry::face_vertices_name);
    REQUIRE(faces);
    CHECK(faces->domain == AttributeDomain::Face);
    CHECK(faces->type() == AttributeType::UInt32);
    CHECK(faces->component_count() == 3);
    CHECK(faces->count() == 2);
    CHECK_FALSE(geo.find_attribute(MeshGeometry::edge_vertices_name));
    const auto edges =
        edges_of(geo)->find_attribute(MeshGeometry::edge_vertices_name);
    REQUIRE(edges);
    CHECK(edges->domain == AttributeDomain::Edge);
    CHECK(edges->component_count() == 2);
    CHECK(edges->count() == 5);

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
    const MeshGeometry copy = geo.clone();
    CHECK(flat_indices(copy.triangle_indices())
          == flat_indices(geo.triangle_indices()));
    CHECK(copy.edge_count() == geo.edge_count());
}

TEST_CASE("MeshGeometry explicit edges override derived", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshGeometry geo = make_quad();
    CHECK(geo.edge_count() == 0);
    CHECK(edges_of(geo)->edge_count() == 5);

    // The edges view shows the geometry's own edges once it has some.
    const std::array<uint32_t, 4> edges = {0, 1, 2, 3};
    geo.set_edge_indices(EdgeIndices(edges.data(), edges.size() / 2));
    CHECK(geo.edge_count() == 2);
    CHECK(edges_of(geo)->edge_count() == 2);

    // Re-setting triangles keeps them.
    const std::array<uint32_t, 3> tri = {0, 1, 2};
    geo.set_triangle_indices(TriangleIndices(tri.data(), tri.size() / 3));
    CHECK(edges_of(geo)->edge_count() == 2);
}

TEST_CASE("BasicShading binds raw scalar attributes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &sh = m.shading;
    m.geometry = make_quad();
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    const auto uv_attr = m.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv);

    // No default: the scalar role is unbound until assigned.
    CHECK_FALSE(bound(sh, roles::scalar()).valid());

    // The binding is the raw attribute; reducing it to a scalar is the
    // renderer's (shader's) job.
    REQUIRE(sh.assign_layer_role(surface_id(sh), roles::scalar(), "uv"));
    const auto b = bound(sh, roles::scalar());
    CHECK(b == uv_attr);
    CHECK(b->component_count() == 2);
    CHECK(flat_values<float>(*b) == std::vector<float>(uv.begin(), uv.end()));

    // Outside the renderer's 1-4 component range: rejected, binding kept.
    const std::array<float, 20> wide{};
    m.geometry.add_attribute("wide", AttributeDomain::Vertex, 5, wide);
    CHECK_FALSE(sh.assign_layer_role(surface_id(sh), roles::scalar(), "wide"));
    CHECK(bound(sh, roles::scalar()) == uv_attr);

    CHECK_FALSE(sh.assign_layer_role(surface_id(sh), roles::scalar(), "missing"));
    // A role the renderer does not read.
    const auto unread = mesh_roles().add({"test.unread", "Unread"});
    CHECK_FALSE(sh.assign_layer_role(surface_id(sh), unread, "uv"));
    CHECK_FALSE(sh.assign_layer_role(MeshLayerId{999}, roles::scalar(), "uv"));
}

TEST_CASE("BasicShading follows geometry changes", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m;
    auto &geo = m.geometry;
    auto &sh = m.shading;
    geo = make_quad();
    REQUIRE(bound(sh, roles::position()).valid());

    // Adding more attributes may reallocate storage; binding must follow.
    for (int i = 0; i < 16; ++i) {
        const std::array<float, 4> s = {0, 1, 2, 3};
        geo.add_attribute("s" + std::to_string(i), AttributeDomain::Vertex, 1, s);
    }
    auto pos = geo.find_attribute("vertex_positions");
    REQUIRE(pos);
    CHECK(bound(sh, roles::position()) == pos);

    // Replacing the bound attribute in place keeps handle identity.
    const std::array<float, 6> pos2 = {0, 0, 1, 1, 2, 2};
    geo.add_attribute("vertex_positions", AttributeDomain::Vertex, 2, pos2);
    CHECK(vertex_count(sh) == 3);
    CHECK(bound(sh, roles::position()) == pos);
    CHECK(bound(sh, roles::position())->component_count() == 2);

    // Explicit bindings see replaced values too.
    REQUIRE(sh.assign_layer_role(surface_id(sh), roles::scalar(), "s3"));
    const std::array<float, 3> s3 = {7, 8, 9};
    geo.add_attribute("s3", AttributeDomain::Vertex, 1, s3);
    CHECK(flat_values<float>(*bound(sh, roles::scalar())) == std::vector<float>{7, 8, 9});

    // Removing an attribute unbinds; a held handle stays readable but
    // detached and cannot be bound.
    CHECK(geo.remove_attribute("vertex_positions"));
    CHECK(geo.remove_attribute("s3"));
    CHECK_FALSE(bound(sh, roles::position()).valid());
    CHECK_FALSE(bound(sh, roles::scalar()).valid());
    CHECK(pos->count() == 3);
    CHECK_FALSE(sh.assign_layer_role(surface_id(sh), roles::position(), pos));

    // A same-named attribute appearing later is picked up again.
    geo.add_attribute("s3", AttributeDomain::Vertex, 1, s3);
    CHECK(bound(sh, roles::scalar()).valid());
}

TEST_CASE("MeshGeometry face attributes are stored but not bindable", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject m{make_quad()};
    const std::array<float, 2> area = {0.5f, 0.5f};
    m.geometry.add_attribute("face_area", AttributeDomain::Face, 1, area);

    auto a = m.geometry.find_attribute("face_area");
    REQUIRE(a);
    CHECK(a->domain == AttributeDomain::Face);
    CHECK(a->count() == 2);
    CHECK_FALSE(m.shading.assign_layer_role(surface_id(m.shading), roles::scalar(),
                                            "face_area"));
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

    MeshObject m{balsa::visualization::to_mesh_geometry(obj)};
    CHECK(vertex_count(m.shading) == 4);
    CHECK(m.geometry.triangle_count() == 2);
    CHECK(edges_of(m.geometry)->edge_count() == 5);
    CHECK_FALSE(bound(m.shading, balsa::scene_graph::roles::normal()).valid());
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
        CHECK(edges_of(md)->edge_count() == 5);
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

TEST_CASE("MeshGeometry clone and BasicShading copy_from", "[scene_graph]") {
    using namespace balsa::scene_graph;
    using balsa::visualization::vulkan::NormalSource;

    MeshObject src{make_quad(/*with_normals=*/true)};
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    src.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv);
    REQUIRE(src.shading.assign_layer_role(surface_id(src.shading), roles::scalar(), "uv"));
    REQUIRE(src.shading.clear_layer_role(surface_id(src.shading), roles::normal()));
    auto *src_surface = src.shading.find_layer("balsa.phong_surface");
    REQUIRE(src_surface);
    auto *src_renderer = dynamic_cast<PhongSurfaceRenderer *>(src_surface->renderer.get());
    REQUIRE(src_renderer);
    src_renderer->state.normal_source = NormalSource::None;

    MeshObject dst{src.geometry.clone()};
    dst.shading.copy_from(src.shading);

    // Geometry: same contents, independent attribute instances.
    CHECK(dst.geometry.triangle_count() == 2);
    CHECK(edges_of(dst.geometry)->edge_count() == 5);
    const auto src_uv = src.geometry.find_attribute("uv");
    const auto dst_uv = dst.geometry.find_attribute("uv");
    REQUIRE(dst_uv);
    CHECK_FALSE(dst_uv == src_uv);
    CHECK_FALSE(dst.geometry.owns(src_uv));
    CHECK(flat_values<float>(*dst_uv) == flat_values<float>(*src_uv));

    // Shading: bindings re-targeted by name; cleared roles stay cleared.
    CHECK(bound(dst.shading, roles::position())
          == dst.geometry.find_attribute("vertex_positions"));
    CHECK(bound(dst.shading, roles::scalar()) == dst_uv);
    CHECK(dst.shading.find_layer("balsa.phong_surface")->binding(roles::scalar())->attribute
          == dst_uv);
    CHECK_FALSE(bound(dst.shading, roles::normal()).valid());
    const auto *dst_surface = dst.shading.find_layer("balsa.phong_surface");
    REQUIRE(dst_surface);
    const auto *dst_renderer =
        dynamic_cast<const PhongSurfaceRenderer *>(dst_surface->renderer.get());
    REQUIRE(dst_renderer);
    CHECK(dst_renderer->state.normal_source == NormalSource::None);

    // Changing the copy leaves the source alone.
    const std::array<float, 8> uv2 = {0, 9, 0, 9, 0, 9, 0, 9};
    dst.geometry.add_attribute("uv", AttributeDomain::Vertex, 2, uv2);
    CHECK(flat_values<float>(*bound(dst.shading, roles::scalar()))[1] == 9.0f);
    CHECK(flat_values<float>(*bound(src.shading, roles::scalar()))[1] == 4.0f);
}

TEST_CASE("basic_shading_active_layer", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshObject object{make_quad()};
    auto &shading = object.shading;
    REQUIRE(shading.render_layers().size() >= 2);

    // Defaults to the first layer.
    CHECK(shading.active_layer() == &shading.render_layers().front());

    const auto second = shading.render_layers()[1].id;
    CHECK(shading.set_active_layer(second));
    CHECK(shading.active_layer()->id == second);
    CHECK_FALSE(shading.set_active_layer(MeshLayerId{999}));
    CHECK(shading.active_layer()->id == second);

    // Copies keep it.
    MeshObject copy{object.geometry.clone()};
    copy.shading.copy_from(shading);
    CHECK(copy.shading.active_layer()->id == second);

    // Falls back to the first layer once its layer is gone.
    std::erase_if(shading.render_layers(),
                  [&](const MeshRenderLayer &l) { return l.id == second; });
    CHECK(shading.active_layer() == &shading.render_layers().front());
    shading.render_layers().clear();
    CHECK(shading.active_layer() == nullptr);
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
    *geo = make_single_tri();
    CHECK(vertex_count(sh) == 3);
    CHECK(bound(sh, balsa::scene_graph::roles::position()).valid());
}

TEST_CASE("Objects share geometry with their own shading", "[scene_graph]") {
    using namespace balsa::scene_graph;

    auto geo = std::make_shared<MeshGeometry>();
    *geo = make_quad();
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    geo->add_attribute("uv", AttributeDomain::Vertex, 2, uv);

    Object a("a");
    Object b("b");
    a.emplace_feature<MeshInstance>(geo);
    b.emplace_feature<MeshInstance>(geo);
    auto &sa = a.emplace_feature<BasicShading>();
    auto &sb = b.emplace_feature<BasicShading>();

    // Same attributes, bound independently.
    REQUIRE(sa.assign_layer_role(surface_id(sa), roles::scalar(), "uv"));
    CHECK(bound(sa, roles::scalar()) == geo->find_attribute("uv"));
    CHECK_FALSE(bound(sb, roles::scalar()).valid());

    // A geometry change is seen through both objects.
    const std::array<float, 8> uv2 = {7, 8, 0, 1, 1, 0, 0, 0};
    geo->add_attribute("uv", AttributeDomain::Vertex, 2, uv2);
    CHECK(flat_values<float>(*bound(sa, roles::scalar()))[0] == 7.0f);
    REQUIRE(sb.assign_layer_role(surface_id(sb), roles::scalar(), "uv"));
    CHECK(bound(sb, roles::scalar()) == bound(sa, roles::scalar()));

    // The geometry outlives the objects showing it.
    std::weak_ptr<MeshGeometry> weak = geo;
    geo.reset();
    CHECK_FALSE(weak.expired());

    // Showing different geometry rebinds; roles bound by default follow
    // the new geometry's vertex_positions, explicit ones without a match
    // unbind.
    auto other = std::make_shared<MeshGeometry>();
    *other = make_single_tri();
    a.find_feature<MeshInstance>()->set_geometry(other);
    CHECK(vertex_count(sa) == 3);
    CHECK_FALSE(bound(sa, balsa::scene_graph::roles::scalar()).valid());
    CHECK(vertex_count(sb) == 4);
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
    CHECK(obj.visible() == true);

    obj.set_visible(false);
    CHECK(obj.visible() == false);
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

TEST_CASE("mesh_attribute_view", "[scene_graph][mesh]") {
    MeshObject m;
    const std::vector<float> xyz{0, 1, 2, 3, 4, 5};
    m.geometry.add_attribute("vertex_positions",
                             balsa::scene_graph::AttributeDomain::Vertex, 3,
                             std::span<const float>(xyz), as_positions);
    on_source_view(m.shading);
    const auto b = bound(m.shading, balsa::scene_graph::roles::position());
    REQUIRE(b);
    auto p = b->view<float, 3>();
    REQUIRE(p.has_value());
    CHECK(p->extent(1) == 2);
    CHECK((*p)[1, 0] == 1.0f);
    CHECK((*p)[0, 1] == 3.0f);

    namespace sg = balsa::scene_graph;
    const auto wrong_count = b->view<float, 2>();
    REQUIRE_FALSE(wrong_count);
    const auto *count_error = std::get_if<sg::WrongComponentCount>(&wrong_count.error());
    REQUIRE(count_error);
    CHECK(count_error->actual == 3);
    const auto wrong_type = b->view<double>();
    REQUIRE_FALSE(wrong_type);
    const auto *type_error = std::get_if<sg::WrongType>(&wrong_type.error());
    REQUIRE(type_error);
    CHECK(type_error->actual == sg::AttributeType::Float32);

    auto d = b->view<float>();
    REQUIRE(d);
    CHECK(d->extent(0) == 3);
    CHECK((*d)[2, 1] == 5.0f);
}

TEST_CASE("mesh_attribute_from_mdspan", "[scene_graph][mesh]") {
    using balsa::scene_graph::AttributeType;
    using balsa::scene_graph::AttributeValue;
    using balsa::scene_graph::attribute_type_of;
    STATIC_CHECK(attribute_type_of<float> == AttributeType::Float32);
    STATIC_CHECK(attribute_type_of<const double> == AttributeType::Float64);
    STATIC_CHECK(attribute_type_of<int16_t> == AttributeType::Int16);
    STATIC_CHECK(attribute_type_of<uint8_t> == AttributeType::UInt8);
    STATIC_CHECK(attribute_type_of<std::float16_t> == AttributeType::Float16);
    STATIC_CHECK(AttributeValue<const float>);
    STATIC_CHECK_FALSE(AttributeValue<bool>);
    STATIC_CHECK(attribute_type_of<int64_t> == AttributeType::Int64);
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

    // Values keep their type: no widening.
    const std::vector<uint8_t> labels{1, 2, 255};
    auto u = md.add_attribute("labels", sg::AttributeDomain::Vertex, 1, labels);
    REQUIRE(u);
    CHECK(u->type() == sg::AttributeType::UInt8);
    CHECK(flat_values<uint8_t>(*u) == std::vector<uint8_t>{1, 2, 255});
    CHECK(u->element_size() == 1);
    CHECK_FALSE(u->view<uint32_t>());

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

TEST_CASE("mesh_attribute_tensor_values", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    using ::zipper::dextents;
    sg::MeshGeometry md;

    // Two 2x3 matrices, column-major per element: element i holds
    // [[a b c] [d e f]] stored as a d b e c f.
    const std::vector<float> m{1, 4, 2, 5, 3, 6, 10, 40, 20, 50, 30, 60};
    auto t = md.add_attribute(
        "frames", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<3>, ::zipper::storage::col_major>(
            m.data(), 2, 3, 2));
    REQUIRE(t);
    CHECK(t->rows() == 2);
    CHECK(t->columns() == 3);
    CHECK(t->component_count() == 6);
    CHECK(t->count() == 2);
    CHECK_FALSE(t->is_vector());
    // Stored element-major, each element column-major: the input as-is.
    CHECK(flat_values<float>(*t) == m);

    const auto tv = t->tensor_view<float, 2, 3>();
    REQUIRE(tv);
    CHECK((*tv)[0, 0, 0] == 1.f);
    CHECK((*tv)[1, 0, 0] == 4.f);
    CHECK((*tv)[0, 2, 0] == 3.f);
    CHECK((*tv)[1, 2, 1] == 60.f);
    const auto dynamic = t->tensor_view<float>();
    REQUIRE(dynamic);
    CHECK(dynamic->extent(0) == 2);
    CHECK(dynamic->extent(1) == 3);

    const auto wrong_shape = t->tensor_view<float, 3, 2>();
    REQUIRE_FALSE(wrong_shape);
    const auto *shape_error = std::get_if<sg::WrongShape>(&wrong_shape.error());
    REQUIRE(shape_error);
    CHECK(shape_error->actual_rows == 2);
    CHECK(shape_error->actual_columns == 3);
    CHECK(std::holds_alternative<sg::WrongType>(
        t->tensor_view<double>().error()));

    // The flattened view still works: (rows * columns, count).
    const auto flat = t->view<float, 6>();
    REQUIRE(flat);
    CHECK((*flat)[5, 1] == 60.f);

    // Strided input (padded columns, as glTF stores byte matrices): only
    // the values are kept.
    const std::vector<uint8_t> padded{1, 2, 99, 99, 3, 4, 99, 99};
    using Stride = ::zipper::storage::layout_stride;
    auto p = md.add_attribute(
        "padded", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const uint8_t, dextents<3>, Stride>(
            padded.data(),
            Stride::mapping<dextents<3>>(dextents<3>{2, 2, 1},
                                         std::array<std::size_t, 3>{1, 4, 8})));
    REQUIRE(p);
    CHECK(p->type() == sg::AttributeType::UInt8);
    CHECK(flat_values<uint8_t>(*p) == std::vector<uint8_t>{1, 2, 3, 4});
    CHECK(p->column_stride() == 2);
    CHECK(p->element_size() == 4);

    // Vectors through rank 3 are ordinary vectors.
    const std::vector<float> xyz{0, 1, 2, 3, 4, 5};
    auto v = md.add_attribute(
        "v", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<3>, ::zipper::storage::col_major>(
            xyz.data(), 3, 1, 2));
    REQUIRE(v);
    CHECK(v->is_vector());
    CHECK(v->rows() == 3);
    CHECK(v->tensor_view<float, 3, 1>());
    // Rank-1 / rank-2 attributes are N x 1 tensors.
    auto s = md.add_attribute("s", sg::AttributeDomain::Vertex, 1, xyz);
    REQUIRE(s);
    CHECK(s->rows() == 1);
    CHECK(s->columns() == 1);
    CHECK(s->tensor_view<float, 1, 1>());

    // The shape survives copies (MeshGeometry::clone copies attributes).
    const sg::MeshAttribute copy = *t;
    CHECK(copy.columns() == 3);

    // Roles take scalars and vectors only: a 2x2 matrix has 4 components
    // but is not a vec4.
    const sg::MeshRoleRequirement color{
        .role = sg::mesh_roles().add({"test.color", "Color"}),
        .min_components = 4,
        .max_components = 4};
    CHECK_FALSE(color.accepts(*md.add_attribute(
        "m22", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<3>, ::zipper::storage::col_major>(
            xyz.data(), 2, 2, 1))));
    const std::vector<float> rgba{1, 2, 3, 4};
    CHECK(color.accepts(*md.add_attribute("rgba", sg::AttributeDomain::Vertex,
                                          4, rgba)));

    // Too many components per element is rejected.
    CHECK_FALSE(md.add_attribute(
        "huge", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<3>, ::zipper::storage::col_major>(
            xyz.data(), 16, 16, 0)));
}

TEST_CASE("layer_bindings_keep_attribute_type", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    MeshObject mesh;
    auto &geo = mesh.geometry;
    auto &md = mesh.shading;

    // Bindings hand over the attribute itself, in its stored type.
    const std::vector<double> dpos{0.5, 1, 2, 3, 4, 5.25};
    auto d = geo.add_attribute("vertex_positions", sg::AttributeDomain::Vertex, 3, dpos,
                               as_positions);
    on_source_view(md);
    const auto b = bound(md, sg::roles::position());
    REQUIRE(b == d);
    CHECK(b->type() == sg::AttributeType::Float64);
    auto v = b->view<double, 3>();
    REQUIRE(v);
    CHECK((*v)[2, 1] == 5.25);

    const std::vector<uint32_t> pairs{3, 4, 6, 8};
    auto pr = geo.add_attribute("pairs", sg::AttributeDomain::Vertex, 2, pairs);
    REQUIRE(md.assign_layer_role(surface_id(md), sg::roles::scalar(), "pairs"));
    CHECK(bound(md, sg::roles::scalar()) == pr);
    CHECK(bound(md, sg::roles::scalar())->type() == sg::AttributeType::UInt32);
}

#include <balsa/visualization/colormap_list.hpp>

TEST_CASE("scalar_extents_match_shader_reduction", "[scene_graph][renderer]") {
    namespace sg = balsa::scene_graph;
    using Kind = sg::ScalarReduction::Kind;
    MeshObject m{make_quad()};
    // Per vertex: (3, -4), (0, 1), (2, 0), (0, 0)
    const std::array<float, 8> v = {3, -4, 0, 1, 2, 0, 0, 0};
    const auto a = m.geometry.add_attribute("v", sg::AttributeDomain::Vertex, 2, v);
    auto extents = [&](Kind kind, uint8_t value) {
        return sg::scalar_extents(*a, {.kind = kind, .value = value});
    };
    CHECK(extents(Kind::Component, 0) == std::array{0.0f, 3.0f});
    CHECK(extents(Kind::Component, 1) == std::array{-4.0f, 1.0f});
    CHECK(extents(Kind::Component, 2) == std::array{0.0f, 0.0f}); // missing
    CHECK(extents(Kind::Norm, 1) == std::array{0.0f, 7.0f});
    CHECK(extents(Kind::Norm, 2) == std::array{0.0f, 5.0f});
    const auto l3 = extents(Kind::Norm, 3);
    REQUIRE(l3);
    CHECK((*l3)[1] == Catch::Approx(std::cbrt(91.0f)));

    // Any stored type.
    const std::array<uint32_t, 4> u = {7, 2, 9, 4};
    const auto ua = m.geometry.add_attribute("u", sg::AttributeDomain::Vertex, 1, u);
    CHECK(sg::scalar_extents(*ua, {.kind = Kind::Component, .value = 0})
          == std::array{2.0f, 9.0f});
}

TEST_CASE("phong_surface_scalar_colormap", "[scene_graph][renderer]") {
    namespace sg = balsa::scene_graph;
    namespace ctl = balsa::visualization::controls;
    MeshObject m{make_quad()};
    // Per vertex: (3, 4), (0, 1), (1, 0), (0, 0)
    const std::array<float, 8> uv = {3, 4, 0, 1, 1, 0, 0, 0};
    m.geometry.add_attribute("uv", sg::AttributeDomain::Vertex, 2, uv);
    auto *layer = m.shading.find_layer("balsa.phong_surface");
    REQUIRE(layer);
    auto &renderer = dynamic_cast<sg::PhongSurfaceRenderer &>(*layer->renderer);
    const sg::MeshLayerContext context(m.shading, layer->id);

    CHECK(renderer.draw_request().colormap == "MATLAB_jet");
    CHECK(renderer.clone()->requirement(sg::roles::scalar())->max_components == 4);

    // Every setting is driven from the renderer's controls.
    const ctl::Layout layout = renderer.build_controls(context);
    auto find = [&](std::string_view label, auto visit) {
        int seen = 0;
        layout.for_each_property([&](const ctl::Property &p) {
            if (p.label != label) return;
            visit(p);
            ++seen;
        });
        CHECK(seen == 1);
    };
    auto tooltip = [&](std::string_view label) {
        std::string text;
        find(label, [&](const ctl::Property &p) {
            REQUIRE(p.live_tooltip);
            text = p.live_tooltip();
        });
        return text;
    };
    auto range = [&] {
        sg::MeshMaterial material;
        renderer.write_material(material, context);
        return std::array{material.scalar_min, material.scalar_max};
    };

    // Unbound scalar: uniform colour, no colormap settings.
    find("Color", [&](const ctl::Property &p) { CHECK(p.visible_when()); });
    CHECK(tooltip("Max") == "No scalar attribute bound");
    const ctl::Layout *colormap_column = nullptr;
    for (const auto &item : layout.items()) {
        if (const auto *child = std::get_if<std::unique_ptr<ctl::Layout>>(&item);
            child && (*child)->heading() == "Scalar Colormap") {
            colormap_column = child->get();
        }
    }
    REQUIRE(colormap_column);
    CHECK_FALSE(colormap_column->visible_when()());

    // Binding the scalar role is what colours by it; the range defaults to
    // the data's L2-norm extents.
    REQUIRE(m.shading.assign_layer_role(layer->id, sg::roles::scalar(), "uv"));
    find("Color", [&](const ctl::Property &p) { CHECK_FALSE(p.visible_when()); });
    CHECK(colormap_column->visible_when()());
    CHECK(range() == std::array{0.0f, 5.0f});
    CHECK(tooltip("Max") == "Data range: [0, 5]");
    CHECK(tooltip("Min") == tooltip("Fit to Data"));
    find("Max", [&](const ctl::Property &p) {
        CHECK(std::get<ctl::FloatInput>(p.widget).get() == 5.0f);
    });

    // ...and follows the reduction and the data.
    find("Scalar", [&](const ctl::Property &p) {
        const auto &e = std::get<ctl::ChoiceInput>(p.widget);
        CHECK(e.get_index() == 1); // Norm L2
        e.set_index(4);            // Y
    });
    CHECK(range() == std::array{0.0f, 4.0f});
    const std::array<float, 8> uv2 = {0, -2, 0, 6, 0, 0, 0, 0};
    m.geometry.add_attribute("uv", sg::AttributeDomain::Vertex, 2, uv2);
    CHECK(range() == std::array{-2.0f, 6.0f});
    CHECK(tooltip("Max") == "Data range: [-2, 6]");

    // Editing an end fixes the range; the data no longer moves it.
    find("Max", [&](const ctl::Property &p) {
        std::get<ctl::FloatInput>(p.widget).set(10.0f);
    });
    CHECK_FALSE(renderer.scalar_colormap.fit_to_data);
    CHECK(range() == std::array{-2.0f, 10.0f});
    m.geometry.add_attribute("uv", sg::AttributeDomain::Vertex, 2, uv);
    CHECK(range() == std::array{-2.0f, 10.0f});
    CHECK(tooltip("Max") == "Data range: [0, 4]"); // still shows the data

    // Fitting again returns to the data's extents.
    find("Fit to Data", [&](const ctl::Property &p) {
        std::get<ctl::Button>(p.widget).on_click();
    });
    CHECK(renderer.scalar_colormap.fit_to_data);
    CHECK(range() == std::array{0.0f, 4.0f});

    // Colormaps come from the colormap_shaders subproject.
    find("Colormap", [&](const ctl::Property &p) {
        const auto &e = std::get<ctl::ChoiceInput>(p.widget);
        const auto names = balsa::visualization::shaders::colormap_names();
        CHECK(e.options().size() == names.size());
        CHECK(e.options()[static_cast<std::size_t>(e.get_index())] == "MATLAB_jet");
        const int parula = balsa::visualization::find_colormap_index("MATLAB_parula");
        REQUIRE(parula >= 0);
        e.set_index(parula);
    });
    CHECK(renderer.draw_request().colormap == "MATLAB_parula");

    // Clones keep their own settings.
    auto copy = renderer.clone();
    renderer.scalar_colormap.colormap = "MATLAB_jet";
    CHECK(copy->draw_request().colormap == "MATLAB_parula");

    // Unbinding returns to the uniform colour.
    REQUIRE(m.shading.clear_layer_role(layer->id, sg::roles::scalar()));
    find("Color", [&](const ctl::Property &p) { CHECK(p.visible_when()); });
}

#include <balsa/visualization/normalize_mesh.hpp>
TEST_CASE("unit_box_transform_centers_mesh", "[scene_graph][normalize]") {
    MeshObject m;
    // Box from (10, 20, 30) to (14, 22, 31): center (12, 21, 30.5), range 4.
    // Stored as double: normalization works from any attribute type.
    const std::vector<double> xyz{10, 20, 30, 14, 22, 31};
    m.geometry.add_attribute("vertex_positions",
                             balsa::scene_graph::AttributeDomain::Vertex, 3,
                             std::span<const double>(xyz), as_positions);
    const auto xf = balsa::visualization::unit_box_transform(
        *m.geometry.find_attribute("vertex_positions"));
    REQUIRE(xf);
    auto M = xf->to_matrix();
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
    CHECK(ctl::registered_panels().size() == 6);
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
                                3, std::span<const float>(xyz), as_positions);
    on_source_view(mesh.shading);
    ctl::PanelContext ctx{nullptr, &obj};

    auto object_panels = ctl::available_panels(ctx, "object");
    REQUIRE(object_panels.size() == 2);
    CHECK(object_panels[0]->id == "object.object");
    CHECK(ctl::available_panels(ctx, "data").size() == 2);
    CHECK(ctl::available_panels(ctx, "scene").empty()); // no scene

    // get/set round-trip through the Transform panel.
    const ctl::Layout transform = object_panels[1]->build_layout(ctx);
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
    auto *surface = md.find_layer("balsa.phong_surface");
    REQUIRE(surface);
    auto *surface_renderer =
        dynamic_cast<sg::PhongSurfaceRenderer *>(surface->renderer.get());
    REQUIRE(surface_renderer);
    surface_renderer->state.normal_source = NormalSource::None;
    CHECK(ctl::available_panels(ctx, "material").empty());

    // Shading combo: "From Attribute" disabled without normals; setting a
    // value runs constrain().
    const ctl::Layout shading =
        ctl::available_panels(ctx, "data")[1]->build_layout(ctx);
    shading.for_each_property([&](const ctl::Property &p) {
        const auto *e = std::get_if<ctl::ChoiceInput>(&p.widget);
        if (!e || p.label != "Normals") return;
        CHECK_FALSE(e->option_enabled(0));
        e->set_index(0); // FromAttribute without normals -> downgraded
        CHECK(surface_renderer->state.normal_source
              == NormalSource::ComputedInShader);
        ++seen;
    });
    CHECK(seen == 2);

    // Each render layer offers a binding choice per renderer role: the
    // default, "(none)", then the geometry's attributes.
    const ctl::Layout layers =
        ctl::available_panels(ctx, "data")[0]->build_layout(ctx);
    int position_choices = 0;
    layers.for_each_property([&](const ctl::Property &p) {
        const auto *e = std::get_if<ctl::ChoiceInput>(&p.widget);
        if (!e || p.label != sg::roles::position().id()) return;
        if (position_choices++ > 0) return; // surface layer only
        CHECK(e->options()
              == std::vector<std::string>{"(default: Position: vertex_positions)",
                                          "(none)", "vertex_positions"});
        CHECK(e->get_index() == 0);
        e->set_index(1);
        CHECK_FALSE(bound(md, sg::roles::position()).valid());
        e->set_index(2);
        CHECK(e->get_index() == 2);
        CHECK(bound(md, sg::roles::position()).valid());
        e->set_index(0);
        CHECK(surface->binding(sg::roles::position()) == nullptr);
    });
    CHECK(position_choices == 3); // one per default layer
}

TEST_CASE("mesh_scene_resolves_headlight_enabled_state", "[scene_graph][lighting]") {
    balsa::visualization::vulkan::MeshScene scene;

    const auto enabled = scene.resolve_scene_lights();
    CHECK(enabled.enabled);
    CHECK(enabled.light_color == balsa::scene_graph::Vec3f{1.0f, 1.0f, 1.0f});
    CHECK(enabled.light_dir == balsa::scene_graph::Vec3f{0.0f, 0.0f, 1.0f});

    scene.headlight().enabled = false;
    CHECK_FALSE(scene.resolve_scene_lights().enabled);
}

TEST_CASE("mesh_renderer_registry_and_layer_copy", "[scene_graph][renderer]") {
    namespace sg = balsa::scene_graph;

    sg::register_builtin_mesh_renderers();
    const auto definitions = sg::registered_mesh_renderers();
    CHECK(std::ranges::any_of(definitions, [](const auto *definition) {
        return definition->id == "balsa.phong_surface";
    }));

    auto layers = sg::make_default_mesh_render_layers();
    REQUIRE(layers.size() == 3);
    REQUIRE(layers[0].renderer);
    CHECK(layers[0].renderer->id() == "balsa.phong_surface");

    auto copy = layers;
    REQUIRE(copy[0].renderer);
    CHECK(copy[0].renderer.get() != layers[0].renderer.get());
    CHECK(copy[0].renderer->id() == layers[0].renderer->id());

    auto *surface = dynamic_cast<sg::PhongSurfaceRenderer *>(layers[0].renderer.get());
    auto *copied_surface =
        dynamic_cast<sg::PhongSurfaceRenderer *>(copy[0].renderer.get());
    REQUIRE(surface);
    REQUIRE(copied_surface);
    surface->state.normal_source = balsa::visualization::vulkan::NormalSource::None;
    CHECK(copied_surface->state.normal_source
          != balsa::visualization::vulkan::NormalSource::None);
}

TEST_CASE("mesh_layer_raw_role_bindings", "[scene_graph][renderer]") {
    using namespace balsa::scene_graph;

    MeshObject source{make_quad(/*with_normals=*/true)};
    auto *surface = source.shading.find_layer("balsa.phong_surface");
    REQUIRE(surface);
    const auto position = source.geometry.find_attribute("vertex_positions");
    const auto normal = source.geometry.find_attribute("vertex_normals");
    const auto uv = source.geometry.add_attribute(
        "uv", AttributeDomain::Vertex, 2,
        std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1});
    REQUIRE(position);
    REQUIRE(normal);
    REQUIRE(uv);

    REQUIRE(source.shading.assign_layer_role(surface->id,
                                             roles::position(), position));
    REQUIRE(source.shading.assign_layer_role(surface->id,
                                             roles::normal(), normal));
    CHECK(surface->binding(roles::position())->attribute == position);
    CHECK(surface->binding(roles::normal())->attribute == normal);
    const auto wide = source.geometry.add_attribute(
        "wide", AttributeDomain::Vertex, 5, std::array<float, 20>{});
    CHECK_FALSE(source.shading.assign_layer_role(surface->id, roles::position(), wide));
    CHECK(surface->binding(roles::position())->attribute == position);

    // clear: explicitly unbound (no default); reset: back to the default.
    REQUIRE(source.shading.clear_layer_role(surface->id, roles::normal()));
    CHECK_FALSE(source.shading.layer_attribute(*surface, roles::normal()).valid());
    REQUIRE(source.shading.reset_layer_role(surface->id, roles::normal()));
    CHECK(surface->binding(roles::normal()) == nullptr);
    CHECK(source.shading.layer_attribute(*surface, roles::normal()) == normal);
    REQUIRE(source.shading.assign_layer_role(surface->id, roles::scalar(), uv));

    MeshObject copy{source.geometry.clone()};
    copy.shading.copy_from(source.shading);
    const auto *copied_surface = copy.shading.find_layer(surface->id);
    REQUIRE(copied_surface);
    REQUIRE(copied_surface->binding(roles::position()));
    CHECK(copied_surface->binding(roles::position())->attribute
          == copy.geometry.find_attribute("vertex_positions"));
    CHECK_FALSE(copied_surface->binding(roles::position())->attribute == position);
    CHECK(copy.shading.layer_attribute(*copied_surface, roles::scalar())
          == copy.geometry.find_attribute("uv"));
}

TEST_CASE("object_visibility_is_inherited", "[scene_graph]") {
    balsa::scene_graph::Object root("root");
    auto &group = root.add_child("group");
    auto &mesh = group.add_child("mesh");
    CHECK(mesh.visible_in_hierarchy());

    group.set_visible(false); // hiding a container hides what is below it
    CHECK(mesh.visible());
    CHECK_FALSE(mesh.visible_in_hierarchy());
    CHECK_FALSE(group.visible_in_hierarchy());
    CHECK(root.visible_in_hierarchy());

    group.set_visible(true);
    mesh.set_visible(false); // and a hidden leaf stays hidden on its own
    CHECK_FALSE(mesh.visible_in_hierarchy());
    CHECK(group.visible_in_hierarchy());

    mesh.set_visible(true);
    root.set_visible(false);
    CHECK_FALSE(mesh.visible_in_hierarchy());
}

TEST_CASE("object_world_state_is_cached_and_invalidated", "[scene_graph]") {
    namespace sg = balsa::scene_graph;
    using V3 = sg::Vec3f;
    // World position of `object`'s origin.
    const auto origin = [](const sg::Object &object) {
        const auto p = (object.world_transform()
                        * ::zipper::Vector<float, 3>{0.f, 0.f, 0.f}.homogeneous())
                           .eval();
        return V3{p(0), p(1), p(2)};
    };
    const auto near = [](const V3 &a, const V3 &b) {
        return (a - b).norm() < 1e-5f;
    };

    sg::Object root("root");
    auto &parent = root.add_child("parent");
    auto &child = parent.add_child("child");
    child.set_translation(V3{1.f, 0.f, 0.f});
    CHECK(near(origin(child), V3{1.f, 0.f, 0.f}));

    // Every way of changing an ancestor reaches the (cached) child.
    parent.set_translation(V3{0.f, 2.f, 0.f});
    CHECK(near(origin(child), V3{1.f, 2.f, 0.f}));
    parent.translate(V3{0.f, 1.f, 0.f});
    CHECK(near(origin(child), V3{1.f, 3.f, 0.f}));
    parent.set_uniform_scale(2.f);
    CHECK(near(origin(child), V3{2.f, 3.f, 0.f}));
    parent.set_scale_factors(V3{3.f, 1.f, 1.f});
    CHECK(near(origin(child), V3{3.f, 3.f, 0.f}));
    parent.reset_transform();
    CHECK(near(origin(child), V3{1.f, 0.f, 0.f}));
    // 90 degrees about z: x -> y.
    parent.set_rotation(sg::Quaternionf(0.7071068f, 0.f, 0.f, 0.7071068f));
    CHECK(near(origin(child), V3{0.f, 1.f, 0.f}));
    parent.rotate(sg::Quaternionf(0.7071068f, 0.f, 0.f, 0.7071068f));
    CHECK(near(origin(child), V3{-1.f, 0.f, 0.f}));
    parent.set_rotation_euler(V3{0.f, 0.f, 0.f});
    CHECK(near(origin(child), V3{1.f, 0.f, 0.f}));
    sg::AffineTransformf up;
    up.translation() = V3{0.f, 0.f, 5.f};
    root.set_from_transform(up);
    CHECK(near(origin(child), V3{1.f, 0.f, 5.f})); // through two levels

    // Visibility is cached the same way.
    CHECK(child.visible_in_hierarchy());
    root.set_visible(false);
    CHECK_FALSE(child.visible_in_hierarchy());
    root.set_visible(true);
    CHECK(child.visible_in_hierarchy());

    // Reparenting and detaching.
    auto &other = root.add_child("other");
    other.set_translation(V3{0.f, 10.f, 0.f});
    CHECK(near(origin(child), V3{1.f, 0.f, 5.f}));
    auto &moved = other.add_child(child.detach());
    CHECK(near(origin(moved), V3{1.f, 10.f, 5.f}));
    other.set_visible(false);
    CHECK_FALSE(moved.visible_in_hierarchy());
    const auto detached = moved.detach();
    CHECK(near(origin(*detached), V3{1.f, 0.f, 0.f})); // just its own
    CHECK(detached->visible_in_hierarchy());

    // Moving an Object (and so its subtree) keeps world state consistent.
    sg::Object source("source");
    source.set_translation(V3{0.f, 0.f, 7.f});
    auto &leaf = source.add_child("leaf");
    leaf.set_translation(V3{1.f, 0.f, 0.f});
    CHECK(near(origin(leaf), V3{1.f, 0.f, 7.f}));
    sg::Object target(std::move(source));
    target.set_translation(V3{0.f, 0.f, 1.f});
    CHECK(near(origin(*target.children()[0]), V3{1.f, 0.f, 1.f}));

    // Reading without changes returns the same cached transform.
    const auto *first = &child.world_transform();
    CHECK(&child.world_transform() == first);
}

TEST_CASE("mesh_attribute_exact_types", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    using ::zipper::dextents;
    using sg::AttributeType;
    sg::MeshGeometry md;

    // Every supported type is stored as given.
    const auto stored = [&]<typename T>(std::string name) {
        const std::vector<T> values{T(1), T(2), T(3), T(4)};
        const auto a = md.add_attribute(
            std::move(name), sg::AttributeDomain::Vertex,
            ::zipper::mdspan<const T, dextents<2>, ::zipper::storage::col_major>(
                values.data(), 2, 2));
        REQUIRE(a);
        CHECK(flat_values<T>(*a) == values);
        CHECK(a->element_size() == 2 * sizeof(T));
        CHECK(a->column_stride() == 2 * sizeof(T));
        return a->type();
    };
    CHECK(stored.operator()<std::float16_t>("f16") == AttributeType::Float16);
    CHECK(stored.operator()<float>("f32") == AttributeType::Float32);
    CHECK(stored.operator()<double>("f64") == AttributeType::Float64);
    CHECK(stored.operator()<int8_t>("i8") == AttributeType::Int8);
    CHECK(stored.operator()<int16_t>("i16") == AttributeType::Int16);
    CHECK(stored.operator()<int32_t>("i32") == AttributeType::Int32);
    CHECK(stored.operator()<uint8_t>("u8") == AttributeType::UInt8);
    CHECK(stored.operator()<uint16_t>("u16") == AttributeType::UInt16);
    CHECK(stored.operator()<uint32_t>("u32") == AttributeType::UInt32);
    CHECK(stored.operator()<int64_t>("i64") == AttributeType::Int64);
    CHECK(stored.operator()<uint64_t>("u64") == AttributeType::UInt64);

    // Values follow VkComponentTypeKHR.
    STATIC_CHECK(std::to_underlying(AttributeType::Float16) == 0);
    STATIC_CHECK(std::to_underlying(AttributeType::Int8) == 3);
    STATIC_CHECK(std::to_underlying(AttributeType::UInt8) == 7);
    STATIC_CHECK(std::to_underlying(AttributeType::UInt32) == 9);
    STATIC_CHECK(sg::attribute_type_size(AttributeType::Float64) == 8);
    STATIC_CHECK(sg::attribute_type_size(AttributeType::Float16) == 2);
    STATIC_CHECK(sg::attribute_type_of<uint64_t> == AttributeType::UInt64);
    STATIC_CHECK(sg::attribute_type_size(AttributeType::Int64) == 8);
    STATIC_CHECK_FALSE(sg::AttributeValue<bool>);

    // Normalized 8 / 16-bit integers keep their bytes and the flag.
    const std::vector<uint8_t> rgba{255, 0, 128, 255};
    const auto color = md.add_attribute(
        "color", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const uint8_t, dextents<2>, ::zipper::storage::col_major>(
            rgba.data(), 4, 1),
        {.normalized = true});
    REQUIRE(color);
    CHECK(color->normalized());
    CHECK(color->type() == AttributeType::UInt8);
    CHECK(flat_values<uint8_t>(*color) == rgba);
    CHECK_FALSE(md.find_attribute("f32")->normalized());

    // Only 8 / 16-bit integers can be normalized.
    const std::vector<float> xyz{0, 1, 2};
    CHECK_FALSE(md.add_attribute(
        "bad", sg::AttributeDomain::Vertex,
        ::zipper::mdspan<const float, dextents<1>>(xyz.data(), 3),
        {.normalized = true}));
}

#include <balsa/visualization/vulkan/vertex_format.hpp>

TEST_CASE("vertex_format_from_attribute_type", "[vulkan][mesh]") {
    namespace bvk = balsa::visualization::vulkan;
    using T = balsa::scene_graph::AttributeType;
    using F = vk::Format;
    CHECK(bvk::vertex_format(T::Float32, 3, false) == F::eR32G32B32Sfloat);
    CHECK(bvk::vertex_format(T::Float16, 2, false) == F::eR16G16Sfloat);
    CHECK(bvk::vertex_format(T::UInt8, 4, true) == F::eR8G8B8A8Unorm);
    CHECK(bvk::vertex_format(T::Int16, 2, true) == F::eR16G16Snorm);
    CHECK(bvk::vertex_format(T::UInt8, 1, false) == F::eR8Uscaled);
    CHECK(bvk::vertex_format(T::Int16, 3, false) == F::eR16G16B16Sscaled);
    // None that a float shader input can read.
    CHECK_FALSE(bvk::vertex_format(T::Int32, 1, false));
    CHECK_FALSE(bvk::vertex_format(T::Int64, 1, false));
    CHECK_FALSE(bvk::vertex_format(T::UInt32, 1, false));
    CHECK_FALSE(bvk::vertex_format(T::Float64, 3, false));
    CHECK_FALSE(bvk::vertex_format(T::Float32, 5, false));
    CHECK_FALSE(bvk::vertex_format(T::Float32, 0, false));

    CHECK(bvk::vertex_format_size(F::eR32G32B32Sfloat) == 12);
    CHECK(bvk::vertex_format_size(F::eR8G8B8A8Unorm) == 4);
    CHECK(bvk::vertex_format_size(F::eR16G16B16Sscaled) == 6);
    CHECK(bvk::vertex_format_size(F::eD32Sfloat) == 0);
}

TEST_CASE("attribute_semantic_registry", "[scene_graph][semantics]") {
    namespace sg = balsa::scene_graph;
    using ::zipper::dextents;

    // Built-ins are found by id; handles compare by identity.
    CHECK(sg::attribute_semantics().find("balsa.position") == sg::semantics::position());
    CHECK(sg::semantics::position().value().label == "Position");
    CHECK(sg::semantics::position().id() == "balsa.position");
    CHECK(sg::semantics::position() != sg::semantics::normal());
    CHECK_FALSE(sg::attribute_semantics().find("test.unregistered"));

    // A user semantic for per-face 2 x 2 tensors.
    const auto metric_semantic = sg::attribute_semantics().add(
        {.id = "test.metric",
         .label = "Metric",
         .domain = sg::AttributeDomain::Face,
         .accepts = [](const sg::MeshAttribute &a) {
             return a.rows() == 2 && a.columns() == 2;
         }});
    CHECK(sg::attribute_semantics().find("test.metric") == metric_semantic);
    CHECK(std::ranges::contains(sg::attribute_semantics().all(), metric_semantic));

    sg::MeshGeometry geometry;
    const std::vector<float> identity{1, 0, 0, 1};
    const auto metric = geometry.add_attribute(
        "metric", sg::AttributeDomain::Face,
        ::zipper::mdspan<const float, dextents<3>, ::zipper::storage::col_major>(
            identity.data(), 2, 2, 1),
        {.semantic = metric_semantic});
    REQUIRE(metric);
    CHECK(geometry.attribute_for(metric_semantic) == metric);

    // Shape and domain are checked.
    const std::vector<float> vec{1, 2, 3, 4};
    const auto flat = geometry.add_attribute("flat", sg::AttributeDomain::Face, 4, vec);
    CHECK_FALSE(geometry.set_semantic(metric_semantic, flat));
    const auto vertex = geometry.add_attribute("v", sg::AttributeDomain::Vertex, 4, vec);
    CHECK_FALSE(geometry.set_semantic(metric_semantic, vertex));

    // Re-registering replaces the definition behind existing handles.
    const auto replaced = sg::attribute_semantics().add(
        {.id = "test.metric", .label = "Metric 2", .domain = sg::AttributeDomain::Face});
    CHECK(replaced == metric_semantic);
    CHECK(metric_semantic.value().label == "Metric 2");
    CHECK(geometry.set_semantic(metric_semantic, flat));
}

TEST_CASE("mesh_geometry_semantics", "[scene_graph][semantics]") {
    namespace sg = balsa::scene_graph;
    sg::MeshGeometry geometry;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::vector<float> rgba{1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};

    // Names carry no meaning: "P" is the position once tagged.
    const auto p = geometry.add_attribute("P", sg::AttributeDomain::Vertex, 3, xyz,
                                          {.semantic = sg::semantics::position()});
    REQUIRE(p);
    CHECK(geometry.attribute_for(sg::semantics::position()) == p);
    CHECK_FALSE(geometry.attribute_for(sg::semantics::normal()));

    // Sets, and one attribute under several semantics.
    const auto c = geometry.add_attribute("c", sg::AttributeDomain::Vertex, 4, rgba,
                                          {.semantic = sg::semantics::color(),
                                           .set = 1});
    CHECK(geometry.attribute_for(sg::semantics::color(), 1) == c);
    CHECK_FALSE(geometry.attribute_for(sg::semantics::color(), 0));
    const auto version = geometry.version();
    CHECK(geometry.set_semantic(sg::semantics::color(), c));
    CHECK(geometry.version() > version);
    CHECK(geometry.semantics().size() == 3);

    // A semantic the attribute cannot carry is skipped; the attribute stays.
    const auto bad = geometry.add_attribute("bad", sg::AttributeDomain::Vertex, 4, rgba,
                                            {.semantic = sg::semantics::normal()});
    CHECK(bad);
    CHECK_FALSE(geometry.attribute_for(sg::semantics::normal()));

    // Only this geometry's attributes can be tagged.
    sg::MeshGeometry other;
    const auto foreign = other.add_attribute("n", sg::AttributeDomain::Vertex, 3, xyz);
    CHECK_FALSE(geometry.set_semantic(sg::semantics::normal(), foreign));

    // Replacing values keeps the tag while they still fit, drops it if not.
    const std::vector<float> xy{0, 0, 1, 0, 0, 1};
    geometry.add_attribute("P", sg::AttributeDomain::Vertex, 2, xy);
    CHECK(geometry.attribute_for(sg::semantics::position()) == p);
    const std::vector<float> xyzw(12, 0.f);
    geometry.add_attribute("P", sg::AttributeDomain::Vertex, 4, xyzw);
    CHECK_FALSE(geometry.attribute_for(sg::semantics::position()));

    // Clones point at their own attributes.
    CHECK(geometry.set_semantic(sg::semantics::position(),
                                geometry.add_attribute("Q", sg::AttributeDomain::Vertex,
                                                       3, xyz)));
    const auto copy = geometry.clone();
    REQUIRE(copy.attribute_for(sg::semantics::position()));
    CHECK(copy.owns(copy.attribute_for(sg::semantics::position())));
    CHECK(copy.attribute_for(sg::semantics::position())->name == "Q");
    CHECK(copy.semantics().size() == geometry.semantics().size());

    // Removing an attribute removes its entries; clear removes all.
    CHECK(geometry.clear_semantic(sg::semantics::color(), 1));
    CHECK_FALSE(geometry.clear_semantic(sg::semantics::color(), 1));
    CHECK(geometry.remove_attribute("c"));
    CHECK_FALSE(geometry.attribute_for(sg::semantics::color()));
    geometry.clear();
    CHECK(geometry.semantics().empty());
}

TEST_CASE("roles_resolve_by_semantic", "[scene_graph][semantics]") {
    namespace sg = balsa::scene_graph;
    MeshObject mesh;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};

    on_source_view(mesh.shading);
    // The conventional name alone binds nothing.
    mesh.geometry.add_attribute("vertex_positions", sg::AttributeDomain::Vertex, 3, xyz);
    CHECK_FALSE(bound(mesh.shading, sg::roles::position()));

    const auto p = mesh.geometry.add_attribute("points", sg::AttributeDomain::Vertex, 3,
                                               xyz, as_positions);
    CHECK(bound(mesh.shading, sg::roles::position()) == p);

    const auto *surface = mesh.shading.find_layer("balsa.phong_surface");
    REQUIRE(surface);
    REQUIRE(surface->renderer->requirement(sg::roles::normal()));
    CHECK(surface->renderer->requirement(sg::roles::normal())->semantic
          == sg::semantics::normal());
}

TEST_CASE("fit_to_geometry", "[scene_graph][semantics]") {
    namespace sg = balsa::scene_graph;
    using balsa::visualization::vulkan::NormalSource;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::array<uint32_t, 3> tri{0, 1, 2};
    const std::array<uint32_t, 2> edge{0, 1};
    const auto enabled = [](const sg::BasicShading &shading, std::string_view id) {
        return shading.find_layer(id)->enabled;
    };
    const auto surface = [](sg::BasicShading &shading) {
        return dynamic_cast<sg::PhongSurfaceRenderer *>(
            shading.find_layer("balsa.phong_surface")->renderer.get());
    };

    SECTION("triangles without normals keep every layer, unlit by attribute") {
        MeshObject mesh;
        mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
        mesh.geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));
        mesh.shading.fit_to_geometry();
        CHECK(enabled(mesh.shading, "balsa.phong_surface"));
        CHECK(enabled(mesh.shading, "balsa.unlit_wireframe"));
        CHECK(enabled(mesh.shading, "balsa.unlit_points"));
        CHECK(surface(mesh.shading)->state.normal_source != NormalSource::FromAttribute);
    }

    SECTION("normals are used once tagged") {
        MeshObject mesh;
        mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
        mesh.geometry.add_attribute("n", sg::AttributeDomain::Vertex, 3, xyz, as_normals);
        mesh.geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));
        mesh.shading.fit_to_geometry();
        CHECK(surface(mesh.shading)->state.normal_source == NormalSource::FromAttribute);
    }

    SECTION("edges only: no surface") {
        MeshObject mesh;
        mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
        mesh.geometry.set_edge_indices(DynIdx(edge.data(), 1, 2));
        mesh.shading.fit_to_geometry();
        CHECK_FALSE(enabled(mesh.shading, "balsa.phong_surface"));
        CHECK(enabled(mesh.shading, "balsa.unlit_wireframe"));
        CHECK(enabled(mesh.shading, "balsa.unlit_points"));
    }

    SECTION("no positions: nothing to draw") {
        MeshObject mesh;
        mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz);
        mesh.geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));
        mesh.shading.fit_to_geometry();
        CHECK_FALSE(enabled(mesh.shading, "balsa.phong_surface"));
        CHECK_FALSE(enabled(mesh.shading, "balsa.unlit_wireframe"));
        CHECK_FALSE(enabled(mesh.shading, "balsa.unlit_points"));
    }

    SECTION("layers are never enabled") {
        MeshObject mesh;
        mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
        mesh.geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));
        mesh.shading.find_layer("balsa.unlit_points")->enabled = false;
        mesh.shading.fit_to_geometry();
        CHECK_FALSE(enabled(mesh.shading, "balsa.unlit_points"));
    }
}

TEST_CASE("role_candidates", "[scene_graph][semantics]") {
    namespace sg = balsa::scene_graph;
    MeshObject mesh;
    auto &geometry = mesh.geometry;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::vector<float> rgba(12, 0.5f);
    const std::array<uint32_t, 3> tri{0, 1, 2};

    const auto other = geometry.add_attribute("other", sg::AttributeDomain::Vertex, 3, xyz);
    const auto second = geometry.add_attribute("n1", sg::AttributeDomain::Vertex, 3, xyz,
                                               {.semantic = sg::semantics::normal(),
                                                .set = 1});
    const auto first = geometry.add_attribute("n0", sg::AttributeDomain::Vertex, 3, xyz,
                                              as_normals);
    const auto wide = geometry.add_attribute("wide", sg::AttributeDomain::Vertex, 4, rgba);
    geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));

    // Tagged by set first, then any other attribute the role accepts;
    // the normal role takes 2-4 components, never topology.
    const auto *layer = mesh.shading.find_layer("balsa.phong_surface");
    REQUIRE(layer);
    CHECK(mesh.shading.candidates(*layer, sg::roles::normal())
          == std::vector<sg::MeshAttributeHandle>{first, second, other, wide});

    // The same through a layer context, which also exposes the geometry.
    const sg::MeshLayerContext context(mesh.shading, layer->id);
    CHECK(context.geometry() == &geometry);
    CHECK(context.candidates(sg::roles::normal()).front() == first);
    CHECK(context.candidates(sg::mesh_roles().add({"test.unread", "Unread"})).empty());
}

TEST_CASE("mesh_geometry_simplices", "[scene_graph][mesh]") {
    namespace sg = balsa::scene_graph;
    using Idx = ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>;

    sg::MeshGeometry geometry;
    CHECK(geometry.dimension() == 0);
    CHECK_FALSE(geometry.has_simplices(1));
    CHECK(geometry.simplex_count(7) == 0);

    // Two tetrahedra, one per row; only what is set is stored.
    const std::array<uint32_t, 8> tets{0, 1, 2, 3, 1, 2, 3, 4};
    REQUIRE(geometry.set_simplices<3>(Idx(tets.data(), 2, 4)));
    CHECK(geometry.dimension() == 3);
    CHECK(geometry.simplex_count(3) == 2);
    CHECK((geometry.simplices<3>()[3, 1]) == 4);
    CHECK_FALSE(geometry.has_triangle_indices());
    CHECK_FALSE(geometry.has_edge_indices());

    // Per-cell data lives in the Cell domain.
    const std::vector<float> volume{1.f, 2.f};
    CHECK(geometry.add_attribute("volume", sg::AttributeDomain::Cell, 1, volume));
    CHECK(geometry.find_attribute(sg::MeshGeometry::cell_vertices_name)->domain
          == sg::AttributeDomain::Cell);

    // Wrong arity is rejected.
    CHECK_FALSE(geometry.set_simplices<3>(Idx(tets.data(), 2, 3)));

    // The aliases are dimension 2 and 1; edges come from the edges view.
    sg::MeshGeometry surface;
    const std::array<uint32_t, 3> tri{0, 1, 2};
    REQUIRE(surface.set_triangle_indices(Idx(tri.data(), 1, 3)));
    CHECK(surface.dimension() == 2);
    CHECK(surface.simplex_count(2) == surface.triangle_count());
    CHECK_FALSE(surface.has_edge_indices());
    CHECK(edges_of(surface)->edge_count() == 3);
    CHECK(surface.clone().dimension() == 2);
}

TEST_CASE("mesh_instance_views", "[scene_graph][views]") {
    namespace sg = balsa::scene_graph;
    auto geometry = std::make_shared<sg::MeshGeometry>();
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    geometry->add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
    sg::MeshInstance instance(geometry);

    // The source view is the geometry itself.
    CHECK(sg::geometry_views().find("balsa.source") == sg::views::source());
    CHECK(instance.view(sg::views::source()) == geometry);
    CHECK_FALSE(sg::geometry_views().find("test.unknown"));
    CHECK_FALSE(sg::MeshInstance{}.view(sg::views::source()));

    // Derived once per geometry version, including empty results.
    int derivations = 0;
    const auto copy = sg::geometry_views().add(
        {.id = "test.copy",
         .label = "Copy",
         .derive = [&](const sg::MeshInstance &instance)
             -> std::shared_ptr<const sg::MeshGeometry> {
             ++derivations;
             return std::make_shared<sg::MeshGeometry>(instance.geometry()->clone());
         }});
    const auto nothing = sg::geometry_views().add(
        {.id = "test.nothing",
         .label = "Nothing",
         .derive = [&](const sg::MeshInstance &)
             -> std::shared_ptr<const sg::MeshGeometry> {
             ++derivations;
             return nullptr;
         }});
    const auto first = instance.view(copy);
    REQUIRE(first);
    CHECK(first != geometry);
    CHECK(instance.view(copy) == first);
    CHECK_FALSE(instance.view(nothing));
    CHECK_FALSE(instance.view(nothing));
    CHECK(derivations == 2);

    // Changing the geometry rebuilds; replacing it clears the cache.
    geometry->add_attribute("q", sg::AttributeDomain::Vertex, 3, xyz);
    const auto second = instance.view(copy);
    CHECK(second != first);
    CHECK(second->find_attribute("q"));
    instance.set_geometry(std::make_shared<sg::MeshGeometry>());
    CHECK_FALSE(instance.view(copy)->find_attribute("q"));
    CHECK(derivations == 4);
}

TEST_CASE("layers_draw_their_view", "[scene_graph][views]") {
    namespace sg = balsa::scene_graph;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    const std::array<uint32_t, 3> tri{0, 1, 2};

    // A view showing only the source's points: positions, no triangles.
    const auto points_view = sg::geometry_views().add(
        {.id = "test.points",
         .label = "Points",
         .derive = [](const sg::MeshInstance &instance)
             -> std::shared_ptr<const sg::MeshGeometry> {
             auto points = std::make_shared<sg::MeshGeometry>();
             const auto p = instance.geometry()->attribute_for(sg::semantics::position());
             if (!p) return nullptr;
             const auto values = *p->view<float, 3>();
             points->add_attribute("view_points", sg::AttributeDomain::Vertex,
                                   values, as_positions);
             return points;
         }});

    MeshObject mesh;
    const auto p = mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz,
                                               as_positions);
    mesh.geometry.set_triangle_indices(TriangleIndices(tri.data(), 1));

    auto *surface = mesh.shading.find_layer("balsa.phong_surface");
    auto *points = mesh.shading.find_layer("balsa.unlit_points");
    REQUIRE(surface);
    REQUIRE(points);
    CHECK(surface->view == sg::views::source());
    points->view = points_view;

    // Roles, candidates and the context follow each layer's view.
    CHECK(mesh.shading.layer_attribute(*surface, sg::roles::position()) == p);
    const auto view_position = mesh.shading.layer_attribute(*points, sg::roles::position());
    REQUIRE(view_position);
    CHECK(view_position->name == "view_points");
    CHECK(mesh.shading.candidates(*points, sg::roles::position()).front() == view_position);
    const sg::MeshLayerContext context(mesh.shading, points->id);
    CHECK(context.geometry() == mesh.shading.layer_geometry(*points).get());
    CHECK(context.geometry() != &mesh.geometry);

    // Fitting checks the view: a surface on the points view has no triangles.
    surface->view = points_view;
    mesh.shading.fit_to_geometry();
    CHECK_FALSE(surface->enabled);
    CHECK(points->enabled);

    // A view with nothing to show draws nothing.
    points->view = sg::geometry_views().add(
        {.id = "test.empty",
         .label = "Empty",
         .derive = [](const sg::MeshInstance &)
             -> std::shared_ptr<const sg::MeshGeometry> { return nullptr; }});
    CHECK_FALSE(mesh.shading.layer_attribute(*points, sg::roles::position()));
}

TEST_CASE("boundary_and_edges_views", "[scene_graph][views]") {
    namespace sg = balsa::scene_graph;
    using Idx = ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>;
    using Vec3 = ::zipper::Vector<float, 3>;

    // Two positively oriented tetrahedra sharing face (1, 2, 3).
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1};
    const std::array<uint32_t, 8> tets{0, 1, 2, 3, 1, 2, 3, 4};
    auto geometry = std::make_shared<sg::MeshGeometry>();
    const auto p = geometry->add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz,
                                           as_positions);
    const std::vector<float> volume{1, 2};
    geometry->add_attribute("volume", sg::AttributeDomain::Cell, 1, volume);
    REQUIRE(geometry->set_simplices<3>(Idx(tets.data(), 2, 4)));
    sg::MeshInstance instance(geometry);

    const auto boundary = sg::views::boundary();

    // Boundary: the 6 boundary faces (the shared one is interior), on the
    // shared vertex attributes; per-cell data is left out.
    const auto surface = instance.view(boundary);
    REQUIRE(surface);
    CHECK(surface->triangle_count() == 6);
    CHECK_FALSE(surface->has_simplices(3));
    CHECK(surface->attribute_for(sg::semantics::position()) == p);
    CHECK(surface->owns(p));
    CHECK_FALSE(surface->find_attribute("volume"));

    // Every face points away from the centre.
    const auto positions = *p->view<float, 3>();
    const auto point = [&](uint32_t v) {
        return Vec3{positions[0, v], positions[1, v], positions[2, v]};
    };
    const Vec3 centre{0.4f, 0.4f, 0.4f};
    const auto triangles = surface->triangle_indices();
    for (std::size_t t = 0; t < triangles.extent(1); ++t) {
        const auto a = point(triangles[0, t]);
        const auto b = point(triangles[1, t]);
        const auto c = point(triangles[2, t]);
        const Vec3 normal = (b - a).cross(c - a);
        const Vec3 outward = a - centre;
        CHECK(normal.dot(outward) > 0.f);
    }

    // Edges: all of the tets'.
    CHECK(instance.view(sg::views::edges())->edge_count() == 9);
    CHECK(instance.view(sg::views::source()) == geometry);

    // Edges of the boundary, built on the cached boundary.
    const auto boundary_edges = sg::views::boundary_edges();
    const auto edges = instance.view(boundary_edges);
    REQUIRE(edges);
    CHECK(edges->edge_count() == 9);
    CHECK_FALSE(edges->has_triangle_indices());
    CHECK(instance.view(boundary) == surface);

    // A triangle mesh's boundary is its border, wound like its triangles.
    auto quad = std::make_shared<sg::MeshGeometry>(make_quad());
    sg::MeshInstance quad_instance(quad);
    const auto border = quad_instance.view(boundary);
    REQUIRE(border);
    CHECK(border->edge_count() == 4);
    // In no particular order, each running 0 -> 1 -> 2 -> 3 -> 0.
    const auto border_edges = border->edge_indices();
    std::vector<std::array<uint32_t, 2>> wound;
    for (std::size_t e = 0; e < border_edges.extent(1); ++e) {
        wound.push_back({border_edges[0, e], border_edges[1, e]});
    }
    std::ranges::sort(wound);
    CHECK(wound == std::vector<std::array<uint32_t, 2>>{{0, 1}, {1, 2}, {2, 3}, {3, 0}});
    CHECK(quad_instance.view(sg::views::edges())->edge_count() == 5);
    CHECK(quad_instance.view(sg::views::boundary_edges()) == border);

    // A curve's boundary is its endpoints, a vertex subset; a closed
    // curve has none.
    const std::array<uint32_t, 6> path{0, 1, 1, 2, 2, 3};
    auto curve = std::make_shared<sg::MeshGeometry>();
    curve->add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
    REQUIRE(curve->set_simplices<1>(Idx(path.data(), 3, 2)));
    sg::MeshInstance curve_instance(curve);
    const auto ends = curve_instance.view(boundary);
    REQUIRE(ends);
    CHECK(ends->dimension() == 0);
    CHECK_FALSE(ends->has_simplices(1));
    const auto end_vertices = ends->simplices<0>();
    std::vector<uint32_t> end_list(end_vertices.data_handle(),
                                   end_vertices.data_handle() + end_vertices.size());
    std::ranges::sort(end_list);
    CHECK(end_list == std::vector<uint32_t>{0, 3});
    // The subset is topology, not a vertex attribute views share.
    CHECK_FALSE(curve->share_vertex_attributes().has_simplices(0));

    const std::array<uint32_t, 6> loop{0, 1, 1, 2, 2, 0};
    curve->set_simplices<1>(Idx(loop.data(), 3, 2));
    CHECK_FALSE(curve_instance.view(boundary));

    // Points have neither.
    auto points = std::make_shared<sg::MeshGeometry>();
    points->add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
    sg::MeshInstance point_instance(points);
    CHECK_FALSE(point_instance.view(boundary));
    CHECK_FALSE(point_instance.view(sg::views::edges()));

    // Views rebuild when the source changes.
    const std::array<uint32_t, 4> one_tet{0, 1, 2, 3};
    geometry->set_simplices<3>(Idx(one_tet.data(), 1, 4));
    CHECK(instance.view(boundary)->triangle_count() == 4);
    CHECK(instance.view(boundary_edges)->edge_count() == 6);
}

TEST_CASE("default_layers_on_a_tet_mesh", "[scene_graph][views]") {
    namespace sg = balsa::scene_graph;
    using Idx = ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>;
    MeshObject mesh;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    const std::array<uint32_t, 4> tet{0, 1, 2, 3};
    mesh.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
    mesh.geometry.set_simplices<3>(Idx(tet.data(), 1, 4));
    mesh.shading.copy_from(sg::BasicShading(sg::make_default_mesh_render_layers(3)));
    mesh.shading.fit_to_geometry();

    // Surface draws the boundary, wireframe its edges, points the vertices.
    for (const auto *id :
         {"balsa.phong_surface", "balsa.unlit_wireframe", "balsa.unlit_points"}) {
        const auto *layer = mesh.shading.find_layer(id);
        REQUIRE(layer);
        CHECK(layer->enabled);
    }
    const auto *surface = mesh.shading.find_layer("balsa.phong_surface");
    CHECK(mesh.shading.layer_geometry(*surface)->triangle_count() == 4);
}

TEST_CASE("controls_layer_view_choice", "[controls][views]") {
    namespace ctl = balsa::visualization::controls;
    namespace sg = balsa::scene_graph;
    ctl::register_builtin_panels();

    // The View choices, one per layer, in layer order.
    const auto view_choices = [](const ctl::Layout &layout) {
        std::vector<const ctl::ChoiceInput *> choices;
        layout.for_each_property([&](const ctl::Property &p) {
            if (p.label != "View") return;
            if (const auto *e = std::get_if<ctl::ChoiceInput>(&p.widget)) {
                choices.push_back(e);
            }
        });
        return choices;
    };
    // The index of a chain among a choice's options, found by its label.
    const auto index_in = [](const ctl::ChoiceInput &choice, sg::GeometryView view) {
        const auto options = choice.options();
        return static_cast<int>(std::ranges::find(options, view.value().label)
                                - options.begin());
    };
    const auto source = sg::views::source();
    const auto boundary = sg::views::boundary();
    const auto edges = sg::views::edges();

    MeshObject quad{make_quad()};
    ctl::PanelContext ctx{nullptr, &quad.object};
    const ctl::Layout layers = ctl::available_panels(ctx, "data")[0]->build_layout(ctx);
    const auto choices = view_choices(layers);
    REQUIRE(choices.size() == 3); // surface, wireframe, points

    const auto &surface = *choices[0];
    CHECK(surface.get_index() == index_in(surface, source));
    CHECK(choices[1]->get_index() == index_in(*choices[1], edges));
    CHECK(choices[2]->get_index() == index_in(*choices[2], source));
    CHECK(surface.option_enabled(index_in(surface, boundary)));
    CHECK(surface.option_enabled(index_in(surface, edges)));

    surface.set_index(index_in(surface, boundary));
    CHECK(quad.shading.find_layer("balsa.phong_surface")->view == boundary);
    CHECK(surface.get_index() == index_in(surface, boundary));

    // Options are sorted by label, whatever the catalog's order.
    CHECK(std::ranges::is_sorted(surface.options()));

    // A point cloud has no surface or edges to offer.
    MeshObject cloud;
    const std::vector<float> xyz{0, 0, 0, 1, 0, 0, 0, 1, 0};
    cloud.geometry.add_attribute("p", sg::AttributeDomain::Vertex, 3, xyz, as_positions);
    ctl::PanelContext cloud_ctx{nullptr, &cloud.object};
    const ctl::Layout cloud_layers =
        ctl::available_panels(cloud_ctx, "data")[0]->build_layout(cloud_ctx);
    const auto &cloud_surface = *view_choices(cloud_layers)[0];
    CHECK_FALSE(cloud_surface.option_enabled(index_in(cloud_surface, boundary)));
    CHECK_FALSE(cloud_surface.option_enabled(index_in(cloud_surface, edges)));
    CHECK(cloud_surface.option_enabled(index_in(cloud_surface, source)));
}

TEST_CASE("own_view_catalog", "[scene_graph][views]") {
    namespace sg = balsa::scene_graph;

    // A catalog of its own starts empty and stays separate from the default.
    balsa::catalog::Catalog<sg::GeometryViewDefinition> catalog("test view");
    CHECK(catalog.all().empty());
    CHECK_FALSE(catalog.find("balsa.boundary"));

    const auto local = catalog.add(
        {.id = "test.local",
         .label = "Local",
         .derive = [](const sg::MeshInstance &instance) -> std::shared_ptr<const sg::MeshGeometry> {
             return instance.shared_geometry();
         }});
    CHECK(catalog.find("test.local") == local);
    CHECK(catalog.all().size() == 1);
    CHECK_FALSE(sg::geometry_views().find("test.local"));

    // Its entries work like any other.
    auto geometry = std::make_shared<sg::MeshGeometry>(make_single_tri());
    sg::MeshInstance instance(geometry);
    CHECK(instance.view(local) == geometry);

    // Moving the catalog keeps its entries valid.
    auto moved = std::move(catalog);
    CHECK(moved.find("test.local") == local);
    CHECK(local.value().label == "Local");
}

TEST_CASE("catalog_definitions_have_ids", "[scene_graph]") {
    namespace sg = balsa::scene_graph;
    STATIC_CHECK(balsa::catalog::Identified<sg::AttributeSemanticDefinition>);
    STATIC_CHECK(balsa::catalog::Identified<sg::GeometryViewDefinition>);
    STATIC_CHECK(balsa::catalog::Identified<sg::MeshRoleDefinition>);
    struct NoId {
        std::string name;
    };
    STATIC_CHECK_FALSE(balsa::catalog::Identified<NoId>);
    STATIC_CHECK_FALSE(balsa::catalog::Identified<int>);
}
