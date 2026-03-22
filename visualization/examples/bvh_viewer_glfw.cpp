// bvh_viewer_glfw.cpp — GLFW + Vulkan BVH bounding volume visualizer
//
// Usage:  bvh_viewer_glfw [--input path/to/model.obj] [--kdop K] [--depth D]
//         bvh_viewer_glfw path/to/model.obj
//
// Loads an OBJ mesh, builds a BVH with k-DOP bounding volumes over its
// triangles, and visualizes the bounding volumes as wireframe polytopes.
// An ImGui panel controls the tree depth level and k-DOP configuration.
//
// Camera: left-drag orbit, right-drag pan, scroll zoom.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <iostream>
#include <span>
#include <stack>
#include <string>
#include <vector>

#if BALSA_HAS_CLI11
#include <CLI/CLI.hpp>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <balsa/visualization/glfw/vulkan/window.hpp>
#include <balsa/visualization/vulkan/imgui_integration.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/MeshData.hpp>

#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/bounding_box.hpp>

#include <zipper/utils/max_coeff.hpp>

#include <quiver/spatial/KDOP.hpp>
#include <quiver/spatial/KDOPGeometry.hpp>
#include <quiver/spatial/BVH.hpp>

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;
namespace sg = balsa::scene_graph;

// ── BVH node collection at a given depth ────────────────────────────
//
// Traverses the BVH and collects node bounds at the specified depth.
// If a leaf is reached before the target depth, it is collected early.

template<int8_t SpatialDim, int8_t K, typename DirPolicy>
auto collect_bounds_at_depth(
  const quiver::spatial::BVH<SpatialDim, K, DirPolicy> &bvh,
  int target_depth)
  -> std::vector<quiver::spatial::KDOP<SpatialDim, K, DirPolicy>> {

    using kdop_type = quiver::spatial::KDOP<SpatialDim, K, DirPolicy>;
    std::vector<kdop_type> result;

    if (!bvh.is_built() || bvh.node_count() == 0) return result;

    auto nodes = bvh.nodes();

    // DFS with (node_index, depth) pairs
    std::stack<std::pair<uint32_t, int>> stack;
    stack.push({ 0, 0 });

    while (!stack.empty()) {
        auto [idx, depth] = stack.top();
        stack.pop();

        const auto &node = nodes[idx];

        if (depth == target_depth || node.is_leaf()) {
            result.push_back(node.bounds);
        } else {
            // Push right child first so left is processed first
            stack.push({ node.right_child(), depth + 1 });
            stack.push({ idx + 1, depth + 1 });// left child (implicit)
        }
    }

    return result;
}

// ── Convert KDOP geometry into MeshData for wireframe rendering ─────

template<int8_t K, typename DirPolicy>
void set_kdop_wireframe(
  sg::MeshData &mesh_data,
  const std::vector<quiver::spatial::KDOP<3, K, DirPolicy>> &bounds,
  float uniform_color[4]) {

    std::vector<sg::Vec3f> all_positions;
    std::vector<uint32_t> all_edges;

    for (const auto &kdop : bounds) {
        auto geom = quiver::spatial::kdop_geometry(kdop);

        uint32_t base = static_cast<uint32_t>(all_positions.size());

        for (const auto &v : geom.vertices) {
            sg::Vec3f pos;
            pos(0) = static_cast<float>(v[0]);
            pos(1) = static_cast<float>(v[1]);
            pos(2) = static_cast<float>(v[2]);
            all_positions.push_back(pos);
        }

        for (const auto &[a, b] : geom.edges) {
            all_edges.push_back(base + static_cast<uint32_t>(a));
            all_edges.push_back(base + static_cast<uint32_t>(b));
        }
    }

    mesh_data.set_positions(all_positions);
    mesh_data.set_edge_indices(all_edges);

    auto &rs = mesh_data.render_state();
    rs.layers.solid.enabled = false;
    rs.layers.wireframe.enabled = true;
    rs.layers.wireframe.color[0] = uniform_color[0];
    rs.layers.wireframe.color[1] = uniform_color[1];
    rs.layers.wireframe.color[2] = uniform_color[2];
    rs.layers.wireframe.color[3] = uniform_color[3];
    rs.layers.points.enabled = false;
    rs.color_source = vk_viz::ColorSource::Uniform;
    rs.uniform_color[0] = uniform_color[0];
    rs.uniform_color[1] = uniform_color[1];
    rs.uniform_color[2] = uniform_color[2];
    rs.uniform_color[3] = uniform_color[3];
    rs.normal_source = vk_viz::NormalSource::None;
    rs.shading = vk_viz::ShadingModel::Flat;
}

// ── BVH Viewer Scene ────────────────────────────────────────────────
//
// Manages the mesh, BVH construction, and bounding volume visualization.
// Supports switching between AABB (K=3), 9-DOP, and 13-DOP at runtime.

class BVHViewerScene : public vk_viz::MeshScene {
  public:
    BVHViewerScene() = default;
    ~BVHViewerScene() override = default;

    void init_imgui(vk_viz::Film &film, GLFWwindow *glfw_window) {
        _imgui.init(film, glfw_window);
    }

    // Load mesh from OBJ, normalize to unit box, build BVH
    void load_mesh(const std::filesystem::path &path) {
        spdlog::info("Loading OBJ: {}", path.string());

        auto obj = balsa::geometry::triangle_mesh::read_objF(path);
        const auto &pos = obj.position;
        const auto &nrm = obj.normal;

        if (pos.vertices.extent(1) == 0) {
            spdlog::error("OBJ file has no vertices: {}", path.string());
            return;
        }

        _n_verts = pos.vertices.extent(1);
        _n_tris = pos.triangles.extent(1);

        spdlog::info("  {} vertices, {} triangles", _n_verts, _n_tris);

        // Normalize to unit bounding box centered at origin
        balsa::ColVectors<float, 3> V = pos.vertices;
        auto bb = balsa::geometry::bounding_box(V);
        float range = ::zipper::utils::maxCoeff(bb.range());
        if (range < 1e-8f) range = 1.0f;
        balsa::Vector3<float> center = (bb.min() + bb.max()) / 2.0f;
        for (::zipper::index_type j = 0; j < V.extent(1); ++j) {
            auto col = V.col(j);
            col = (col - center) / range;
        }

        // Store vertex positions as doubles for KDOP fitting and as
        // float Vec3f for rendering
        _vertices_f.resize(_n_verts);
        _vertices_d.resize(_n_verts);
        for (size_t j = 0; j < _n_verts; ++j) {
            _vertices_f[j](0) = V(0, j);
            _vertices_f[j](1) = V(1, j);
            _vertices_f[j](2) = V(2, j);
            _vertices_d[j] = {
                static_cast<double>(V(0, j)),
                static_cast<double>(V(1, j)),
                static_cast<double>(V(2, j))
            };
        }

        // Store triangle indices
        _tri_indices.resize(_n_tris * 3);
        for (size_t j = 0; j < _n_tris; ++j) {
            _tri_indices[j * 3 + 0] = static_cast<uint32_t>(pos.triangles(0, j));
            _tri_indices[j * 3 + 1] = static_cast<uint32_t>(pos.triangles(1, j));
            _tri_indices[j * 3 + 2] = static_cast<uint32_t>(pos.triangles(2, j));
        }

        // Store normals if available
        _normals_f.clear();
        _has_normals = (nrm.vertices.extent(1) == pos.vertices.extent(1));
        if (_has_normals) {
            _normals_f.resize(_n_verts);
            for (size_t j = 0; j < _n_verts; ++j) {
                _normals_f[j](0) = nrm.vertices(0, j);
                _normals_f[j](1) = nrm.vertices(1, j);
                _normals_f[j](2) = nrm.vertices(2, j);
            }
        }

        // Store edge indices if available
        _edge_indices.clear();
        if (pos.edges.extent(1) > 0) {
            size_t n_edges = pos.edges.extent(1);
            _edge_indices.resize(n_edges * 2);
            for (size_t j = 0; j < n_edges; ++j) {
                _edge_indices[j * 2 + 0] = static_cast<uint32_t>(pos.edges(0, j));
                _edge_indices[j * 2 + 1] = static_cast<uint32_t>(pos.edges(1, j));
            }
        }

        _mesh_loaded = true;
        _mesh_name = path.filename().string();

        // Build BVH and update scene
        rebuild_bvh();
        update_scene();
    }

    void draw(vk_viz::Film &film) override {
        MeshScene::draw(film);

        if (_imgui.is_initialized()) {
            _imgui.new_frame();
            draw_controls();
            _imgui.render(film);
        }
    }

    void release_vulkan_resources() override {
        _imgui.shutdown();
        MeshScene::release_vulkan_resources();
    }

  private:
    vk_viz::ImGuiIntegration _imgui;

    // Mesh data
    bool _mesh_loaded = false;
    std::string _mesh_name;
    size_t _n_verts = 0;
    size_t _n_tris = 0;
    bool _has_normals = false;
    std::vector<sg::Vec3f> _vertices_f;
    std::vector<std::array<double, 3>> _vertices_d;
    std::vector<sg::Vec3f> _normals_f;
    std::vector<uint32_t> _tri_indices;
    std::vector<uint32_t> _edge_indices;

    // BVH state
    int _kdop_k = 3;// 3 = AABB, 9 = 9-DOP, 13 = 13-DOP
    quiver::spatial::BVH<3, 3> _bvh_3;
    quiver::spatial::BVH<3, 9> _bvh_9;
    quiver::spatial::BVH<3, 13> _bvh_13;
    int _bvh_height = -1;

    // Visualization state
    int _display_depth = 0;
    bool _show_mesh = true;
    float _mesh_color[4] = { 0.85f, 0.85f, 0.85f, 1.0f };
    float _bv_color[4] = { 0.1f, 0.8f, 0.2f, 1.0f };
    float _mesh_alpha = 0.6f;

    // Scene graph objects (not owned — belong to MeshScene's root)
    sg::Object *_mesh_obj = nullptr;
    sg::Object *_bv_obj = nullptr;

    // ── Build per-triangle KDOPs and BVH ────────────────────────────

    template<int8_t K>
    auto build_per_triangle_kdops()
      -> std::vector<quiver::spatial::KDOP<3, K>> {

        using kdop_type = quiver::spatial::KDOP<3, K>;
        std::vector<kdop_type> bounds(_n_tris, kdop_type::empty());

        for (size_t t = 0; t < _n_tris; ++t) {
            uint32_t i0 = _tri_indices[t * 3 + 0];
            uint32_t i1 = _tri_indices[t * 3 + 1];
            uint32_t i2 = _tri_indices[t * 3 + 2];

            std::array<std::span<const double, 3>, 3> pts = {
                std::span<const double, 3>(_vertices_d[i0]),
                std::span<const double, 3>(_vertices_d[i1]),
                std::span<const double, 3>(_vertices_d[i2])
            };

            bounds[t] = kdop_type::fit(pts);
        }

        return bounds;
    }

    void rebuild_bvh() {
        if (!_mesh_loaded || _n_tris == 0) return;

        quiver::spatial::BVHConfig config;
        config.strategy = quiver::spatial::BVHBuildStrategy::sah;
        config.max_leaf_size = 4;

        if (_kdop_k == 3) {
            auto bounds = build_per_triangle_kdops<3>();
            _bvh_3.build(bounds, config);
            _bvh_height = _bvh_3.height();
            spdlog::info("Built AABB BVH: {} nodes, height {}",
                         _bvh_3.node_count(),
                         _bvh_height);
        } else if (_kdop_k == 9) {
            auto bounds = build_per_triangle_kdops<9>();
            _bvh_9.build(bounds, config);
            _bvh_height = _bvh_9.height();
            spdlog::info("Built 9-DOP BVH: {} nodes, height {}",
                         _bvh_9.node_count(),
                         _bvh_height);
        } else {
            auto bounds = build_per_triangle_kdops<13>();
            _bvh_13.build(bounds, config);
            _bvh_height = _bvh_13.height();
            spdlog::info("Built 13-DOP BVH: {} nodes, height {}",
                         _bvh_13.node_count(),
                         _bvh_height);
        }

        _display_depth = std::min(_display_depth, _bvh_height);
    }

    // ── Update scene graph objects ──────────────────────────────────

    void update_scene() {
        // Remove old objects
        if (_mesh_obj) {
            remove_mesh(_mesh_obj);
            _mesh_obj = nullptr;
        }
        if (_bv_obj) {
            remove_mesh(_bv_obj);
            _bv_obj = nullptr;
        }

        if (!_mesh_loaded) return;

        // Add the mesh
        if (_show_mesh) {
            auto &obj = add_mesh(_mesh_name);
            _mesh_obj = &obj;
            auto *md = obj.find_feature<sg::MeshData>();

            md->set_positions(_vertices_f);
            if (_has_normals) {
                md->set_normals(_normals_f);
                md->render_state().normal_source = vk_viz::NormalSource::FromAttribute;
            } else {
                md->render_state().normal_source = vk_viz::NormalSource::None;
                md->render_state().shading = vk_viz::ShadingModel::Flat;
            }

            md->set_triangle_indices(_tri_indices);
            if (!_edge_indices.empty()) {
                md->set_edge_indices(_edge_indices);
            }

            auto &rs = md->render_state();
            rs.layers.solid.enabled = true;
            rs.layers.wireframe.enabled = false;
            rs.layers.points.enabled = false;
            rs.color_source = vk_viz::ColorSource::Uniform;
            rs.uniform_color[0] = _mesh_color[0];
            rs.uniform_color[1] = _mesh_color[1];
            rs.uniform_color[2] = _mesh_color[2];
            rs.uniform_color[3] = _mesh_alpha;
        }

        // Add bounding volume wireframes
        update_bv_visualization();
    }

    void update_bv_visualization() {
        if (_bv_obj) {
            remove_mesh(_bv_obj);
            _bv_obj = nullptr;
        }

        if (!_mesh_loaded || _bvh_height < 0) return;

        auto &obj = add_mesh("BVH Level " + std::to_string(_display_depth));
        _bv_obj = &obj;
        auto *md = obj.find_feature<sg::MeshData>();

        if (_kdop_k == 3) {
            auto bounds = collect_bounds_at_depth(_bvh_3, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        } else if (_kdop_k == 9) {
            auto bounds = collect_bounds_at_depth(_bvh_9, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        } else {
            auto bounds = collect_bounds_at_depth(_bvh_13, _display_depth);
            set_kdop_wireframe(*md, bounds, _bv_color);
        }
    }

    // ── ImGui controls ──────────────────────────────────────────────

    void draw_controls() {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("BVH Bounding Volume Viewer")) {
            ImGui::End();
            return;
        }

        if (!_mesh_loaded) {
            ImGui::TextWrapped("No mesh loaded. Pass --input <file.obj> on the command line.");
            ImGui::End();
            return;
        }

        ImGui::Text("Mesh: %s", _mesh_name.c_str());
        ImGui::Text("Vertices: %zu  Triangles: %zu", _n_verts, _n_tris);
        ImGui::Separator();

        // KDOP type selector
        ImGui::Text("Bounding Volume Type:");
        int prev_k = _kdop_k;
        ImGui::RadioButton("AABB (K=3)", &_kdop_k, 3);
        ImGui::SameLine();
        ImGui::RadioButton("9-DOP", &_kdop_k, 9);
        ImGui::SameLine();
        ImGui::RadioButton("13-DOP", &_kdop_k, 13);

        if (_kdop_k != prev_k) {
            rebuild_bvh();
            update_bv_visualization();
        }

        ImGui::Separator();

        // Tree depth slider
        ImGui::Text("BVH height: %d", _bvh_height);
        int prev_depth = _display_depth;
        ImGui::SliderInt("Display depth", &_display_depth, 0, _bvh_height);
        if (_display_depth != prev_depth) {
            update_bv_visualization();
        }

        // Count of volumes at current level
        size_t bv_count = 0;
        if (_kdop_k == 3)
            bv_count = collect_bounds_at_depth(_bvh_3, _display_depth).size();
        else if (_kdop_k == 9)
            bv_count = collect_bounds_at_depth(_bvh_9, _display_depth).size();
        else
            bv_count = collect_bounds_at_depth(_bvh_13, _display_depth).size();
        ImGui::Text("Bounding volumes at depth %d: %zu", _display_depth, bv_count);

        ImGui::Separator();

        // Appearance controls
        if (ImGui::ColorEdit4("BV Color", _bv_color)) {
            update_bv_visualization();
        }

        bool mesh_changed = false;
        mesh_changed |= ImGui::Checkbox("Show Mesh", &_show_mesh);
        if (_show_mesh) {
            mesh_changed |= ImGui::ColorEdit3("Mesh Color", _mesh_color);
            mesh_changed |= ImGui::SliderFloat("Mesh Alpha", &_mesh_alpha, 0.0f, 1.0f);
        }

        if (mesh_changed) {
            update_scene();
        }

        ImGui::End();
    }
};

// ── BVH Viewer Window ───────────────────────────────────────────────

class BVHViewerWindow : public viz::glfw::vulkan::Window {
  public:
    BVHViewerWindow(const std::string_view &title, int width, int height)
      : viz::glfw::vulkan::Window(title, width, height) {
        _scene = std::make_shared<BVHViewerScene>();
        _scene->init_imgui(film(), glfw_window());

        _camera = std::make_shared<vk_viz::OrbitCameraController>(_scene.get());
        _camera->set_distance(2.5f);
        _camera->set_angles(0.4f, 0.3f);
        set_input_handler(_camera);

        auto fb = framebuffer_size();
        float aspect =
          (fb[1] > 0) ? static_cast<float>(fb[0]) / static_cast<float>(fb[1])
                      : 1.0f;
        constexpr float pi = 3.14159265358979323846f;
        _scene->set_perspective(45.0f * pi / 180.0f, aspect, 0.01f, 100.0f);

        set_scene(_scene);
    }

    void load_obj(const std::filesystem::path &path) { _scene->load_mesh(path); }

  protected:
    void dispatch_mouse(const viz::MouseEvent &e) override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
            return;
        }
        viz::glfw::vulkan::Window::dispatch_mouse(e);
    }

    void dispatch_key(const viz::KeyEvent &e) override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }
        viz::glfw::vulkan::Window::dispatch_key(e);
    }

  private:
    std::shared_ptr<BVHViewerScene> _scene;
    std::shared_ptr<vk_viz::OrbitCameraController> _camera;
};

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);

    std::string input_path;

#if BALSA_HAS_CLI11
    CLI::App app{
        "Balsa BVH Bounding Volume Viewer (GLFW + Vulkan)", "bvh_viewer_glfw"
    };

    app.add_option("input", input_path, "OBJ file to load")
      ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);
#else
    if (argc > 1) {
        input_path = argv[1];
    }
#endif

    glfwInit();

    try {
        BVHViewerWindow window("BVH Bounding Volume Viewer", 1280, 960);

        if (!input_path.empty()) {
            window.load_obj(std::filesystem::path(input_path));
        }

        return window.exec();

    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwTerminate();
    return 0;
}
