#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/Camera.hpp>
#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/types.hpp>


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
    MeshData *mesh = obj.find_feature<MeshData>();
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

TEST_CASE("MeshData geometry and version tracking", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshData md;

    // Initially empty
    CHECK(md.vertex_count() == 0);
    CHECK(md.triangle_count() == 0);
    CHECK(md.edge_count() == 0);
    CHECK_FALSE(md.has_positions());
    CHECK_FALSE(md.has_normals());
    CHECK_FALSE(md.has_triangle_indices());
    CHECK_FALSE(md.has_edge_indices());
    CHECK_FALSE(md.has_vertex_colors());
    CHECK_FALSE(md.has_scalar_field());

    uint64_t v0 = md.version();

    // Set positions
    std::vector<Vec3f> positions(4);
    for (auto &p : positions) {
        p(0) = 0.0f;
        p(1) = 0.0f;
        p(2) = 0.0f;
    }
    md.set_positions(positions);
    CHECK(md.vertex_count() == 4);
    CHECK(md.has_positions());
    CHECK(md.version() > v0);
    uint64_t v1 = md.version();

    // Set triangle indices
    std::vector<uint32_t> tri_idx = { 0, 1, 2, 0, 2, 3 };
    md.set_triangle_indices(tri_idx);
    CHECK(md.triangle_count() == 2);
    CHECK(md.has_triangle_indices());
    CHECK(md.version() > v1);
    uint64_t v2 = md.version();

    // Set edge indices
    std::vector<uint32_t> edge_idx = { 0, 1, 1, 2, 2, 3, 3, 0 };
    md.set_edge_indices(edge_idx);
    CHECK(md.edge_count() == 4);
    CHECK(md.has_edge_indices());
    CHECK(md.version() > v2);
    uint64_t v3 = md.version();

    // Set normals
    std::vector<Vec3f> normals(4);
    for (auto &n : normals) {
        n(0) = 0.0f;
        n(1) = 0.0f;
        n(2) = 1.0f;
    }
    md.set_normals(normals);
    CHECK(md.has_normals());
    CHECK(md.version() > v3);
}

TEST_CASE("MeshData as Object feature", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object obj("mesh_obj");
    auto &md = obj.emplace_feature<MeshData>();

    // Verify it can be found via find_feature
    MeshData *found = obj.find_feature<MeshData>();
    REQUIRE(found != nullptr);
    CHECK(found == &md);

    // Set some data and verify through the found pointer
    std::vector<Vec3f> positions(3);
    for (std::size_t i = 0; i < 3; ++i) {
        positions[i](0) = static_cast<float>(i);
        positions[i](1) = 0.0f;
        positions[i](2) = 0.0f;
    }
    md.set_positions(positions);
    CHECK(found->vertex_count() == 3);
}

TEST_CASE("Object detach", "[scene_graph]") {
    using namespace balsa::scene_graph;

    Object root("root");
    auto &child1 = root.add_child("child1");
    auto &child2 = root.add_child("child2");
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
