#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/Camera.hpp>
#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/types.hpp>

#include <zipper/transform/common.hpp>
#include <zipper/transform/model.hpp>
#include <zipper/transform/quaternion_transform.hpp>


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

TEST_CASE("MeshData auto-derives edges from triangles", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshData md;

    // Set triangle indices for two triangles sharing an edge:
    //   tri 0: (0, 1, 2)
    //   tri 1: (0, 2, 3)
    //
    // Expected unique edges: 0-1, 1-2, 0-2, 2-3, 0-3 = 5 edges
    std::vector<uint32_t> tri_idx = { 0, 1, 2, 0, 2, 3 };
    md.set_triangle_indices(tri_idx);

    CHECK(md.has_triangle_indices());
    CHECK(md.triangle_count() == 2);
    CHECK(md.has_topology());

    // Edges should be auto-derived
    CHECK(md.has_edge_indices());
    CHECK(md.edge_count() == 5);

    // Verify edge indices are valid vertex references
    auto edges = md.edge_indices();
    for (std::size_t i = 0; i < edges.size(); ++i) {
        CHECK(edges[i] < 4);
    }
}

TEST_CASE("MeshData explicit edges override auto-derived", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshData md;

    // Set triangles first (auto-derives 5 edges)
    std::vector<uint32_t> tri_idx = { 0, 1, 2, 0, 2, 3 };
    md.set_triangle_indices(tri_idx);
    CHECK(md.edge_count() == 5);

    // Explicitly set fewer edges — should override
    std::vector<uint32_t> edge_idx = { 0, 1, 2, 3 };
    md.set_edge_indices(edge_idx);
    CHECK(md.edge_count() == 2);

    // Setting triangles again should NOT override explicit edges
    std::vector<uint32_t> tri_idx2 = { 0, 1, 2 };
    md.set_triangle_indices(tri_idx2);
    CHECK(md.edge_count() == 2);// Still the explicit edges
}

TEST_CASE("MeshData topology from single triangle", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshData md;

    std::vector<uint32_t> tri_idx = { 0, 1, 2 };
    md.set_triangle_indices(tri_idx);

    CHECK(md.has_topology());
    CHECK(md.triangle_count() == 1);
    CHECK(md.edge_count() == 3);// A single triangle has 3 edges
}

TEST_CASE("MeshData no topology without triangles", "[scene_graph]") {
    using namespace balsa::scene_graph;

    MeshData md;

    // Only set positions and explicit edges — no topology
    std::vector<Vec3f> positions(4);
    for (auto &p : positions) {
        p(0) = p(1) = p(2) = 0.0f;
    }
    md.set_positions(positions);

    std::vector<uint32_t> edge_idx = { 0, 1, 1, 2 };
    md.set_edge_indices(edge_idx);

    CHECK_FALSE(md.has_topology());
    CHECK(md.has_edge_indices());
    CHECK(md.edge_count() == 2);
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
            CHECK(static_cast<float>(m(r, c)) == Catch::Approx(expected).margin(1e-6f));
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
            CHECK(static_cast<float>(m(r, c)) == Catch::Approx(expected).margin(1e-6f));
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

TEST_CASE("Object translate() adds to current translation", "[scene_graph][trs]") {
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
    euler_deg(0) = 30.0f;// pitch (X)
    euler_deg(1) = 45.0f;// yaw (Y)
    euler_deg(2) = 60.0f;// roll (Z)
    obj.set_rotation_euler(euler_deg);

    Vec3f result = obj.rotation_euler();
    CHECK(static_cast<float>(result(0)) == Catch::Approx(30.0f).margin(0.01f));
    CHECK(static_cast<float>(result(1)) == Catch::Approx(45.0f).margin(0.01f));
    CHECK(static_cast<float>(result(2)) == Catch::Approx(60.0f).margin(0.01f));
}

TEST_CASE("Object euler angle zero is identity rotation", "[scene_graph][trs]") {
    using namespace balsa::scene_graph;

    Object obj("euler_zero");

    Vec3f zero;
    zero(0) = 0.0f;
    zero(1) = 0.0f;
    zero(2) = 0.0f;
    obj.set_rotation_euler(zero);

    // Should produce identity quaternion.
    CHECK(static_cast<float>(obj.rotation().w()) == Catch::Approx(1.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().x()) == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().y()) == Catch::Approx(0.0f).margin(1e-6f));
    CHECK(static_cast<float>(obj.rotation().z()) == Catch::Approx(0.0f).margin(1e-6f));
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
    CHECK(static_cast<float>(dst.translation()(0)) == Catch::Approx(3.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.translation()(1)) == Catch::Approx(-1.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.translation()(2)) == Catch::Approx(7.0f).margin(1e-4f));

    // Scale should match.
    CHECK(static_cast<float>(dst.scale_factors()(0)) == Catch::Approx(2.0f).margin(1e-4f));
    CHECK(static_cast<float>(dst.scale_factors()(1)) == Catch::Approx(0.5f).margin(1e-4f));
    CHECK(static_cast<float>(dst.scale_factors()(2)) == Catch::Approx(1.5f).margin(1e-4f));

    // Euler angles should match (check via re-extraction).
    Vec3f dst_euler = dst.rotation_euler();
    CHECK(static_cast<float>(dst_euler(0)) == Catch::Approx(20.0f).margin(0.1f));
    CHECK(static_cast<float>(dst_euler(1)) == Catch::Approx(35.0f).margin(0.1f));
    CHECK(static_cast<float>(dst_euler(2)) == Catch::Approx(-10.0f).margin(0.1f));
}

TEST_CASE("Object local_transform composes T*R*S correctly", "[scene_graph][trs]") {
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

TEST_CASE("Object world_transform composes parent chain", "[scene_graph][trs]") {
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

TEST_CASE("Object world_transform with rotation propagation", "[scene_graph][trs]") {
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
    CHECK(static_cast<float>(child_world(0, 3)) == Catch::Approx(0.0f).margin(1e-5f));
    CHECK(static_cast<float>(child_world(1, 3)) == Catch::Approx(1.0f).margin(1e-5f));
    CHECK(static_cast<float>(child_world(2, 3)) == Catch::Approx(0.0f).margin(1e-5f));
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
