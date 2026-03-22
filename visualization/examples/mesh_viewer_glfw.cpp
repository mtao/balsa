// mesh_viewer_glfw.cpp — GLFW + Vulkan mesh viewer with ImGui controls
//
// Usage:  mesh_viewer_glfw [--input path/to/model.obj]
//         mesh_viewer_glfw path/to/model.obj
//
// Loads an OBJ mesh, renders it with Phong shading, and provides an
// ImGui panel for tweaking render state (shading model, color source,
// colormaps, lighting, wireframe, etc.).
//
// Camera: left-drag orbit, right-drag pan, scroll zoom.
// File > Open: type a file path in the ImGui dialog to load a mesh.

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
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
#include <balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/MeshData.hpp>

#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/bounding_box.hpp>

#include <zipper/utils/max_coeff.hpp>

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;

// ── MeshViewerScene ─────────────────────────────────────────────────
//
// A MeshScene subclass that adds an ImGui overlay with mesh controls
// and a main menu bar.

class MeshViewerScene : public vk_viz::MeshScene {
  public:
    MeshViewerScene() = default;
    ~MeshViewerScene() override = default;

    // Initialize ImGui after Film is ready.
    void init_imgui(vk_viz::Film &film, GLFWwindow *glfw_window) {
        _imgui.init(film, glfw_window);
    }

    // Set a callback invoked when the user confirms a file path in the
    // ImGui "Open" dialog.
    using OpenFileCallback = std::function<void(const std::filesystem::path &)>;
    void set_open_file_callback(OpenFileCallback cb) { _open_file_cb = std::move(cb); }

    void draw(vk_viz::Film &film) override {
        // Draw mesh geometry first
        MeshScene::draw(film);

        // Then draw ImGui overlay on top
        if (_imgui.is_initialized()) {
            _imgui.new_frame();
            draw_main_menu_bar();
            draw_open_file_dialog();
            vk_viz::imgui::draw_mesh_controls(*this, _panel_state);
            _imgui.render(film);
        }
    }

    void release_vulkan_resources() override {
        _imgui.shutdown();
        MeshScene::release_vulkan_resources();
    }

  private:
    vk_viz::ImGuiIntegration _imgui;
    vk_viz::imgui::MeshPanelState _panel_state;
    OpenFileCallback _open_file_cb;

    // ── ImGui "Open File" dialog state ───────────────────────────────
    bool _show_open_dialog = false;
    char _path_buf[1024] = {};
    std::string _open_error;

    void draw_main_menu_bar() {
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                    _show_open_dialog = true;
                    _open_error.clear();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
                    // GLFW windows can check glfwWindowShouldClose — we
                    // just set a flag that the main loop can detect.  But
                    // the simplest approach is std::exit; alternatively the
                    // window could expose a close request.
                    std::exit(0);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Scene Graph", nullptr, &_panel_state.show_scene_panel);
                ImGui::MenuItem("Properties", nullptr, &_panel_state.show_property_panel);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void draw_open_file_dialog() {
        if (!_show_open_dialog) return;

        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Open Mesh File", &_show_open_dialog)) {
            ImGui::Text("Enter OBJ file path:");
            bool enter_pressed = ImGui::InputText(
              "##path", _path_buf, sizeof(_path_buf), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            bool load_clicked = ImGui::Button("Load");

            if (enter_pressed || load_clicked) {
                std::filesystem::path p(_path_buf);
                if (std::filesystem::exists(p)) {
                    if (_open_file_cb) {
                        _open_file_cb(p);
                    }
                    _show_open_dialog = false;
                    _open_error.clear();
                } else {
                    _open_error = "File not found: " + p.string();
                }
            }

            if (!_open_error.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));
                ImGui::TextWrapped("%s", _open_error.c_str());
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    }
};

// ── MeshViewerWindow ────────────────────────────────────────────────
//
// Subclass of glfw::vulkan::Window that:
//   1. Overrides dispatch_mouse / dispatch_key to let ImGui consume
//      events before they reach the OrbitCameraController.
//   2. Owns the MeshViewerScene and provides load_obj().

class MeshViewerWindow : public viz::glfw::vulkan::Window {
  public:
    MeshViewerWindow(const std::string_view &title, int width, int height)
      : viz::glfw::vulkan::Window(title, width, height) {
        _scene = std::make_shared<MeshViewerScene>();

        // Initialize ImGui (needs Film + GLFWwindow)
        _scene->init_imgui(film(), glfw_window());

        // Wire up the "Open" dialog callback
        _scene->set_open_file_callback([this](const std::filesystem::path &p) {
            load_obj(p);
        });

        // Set up orbit camera
        _camera = std::make_shared<vk_viz::OrbitCameraController>(_scene.get());
        _camera->set_distance(2.5f);
        _camera->set_angles(0.4f, 0.3f);
        set_input_handler(_camera);

        // Set initial projection
        auto fb = framebuffer_size();
        float aspect = (fb[1] > 0) ? static_cast<float>(fb[0]) / static_cast<float>(fb[1]) : 1.0f;
        constexpr float pi = 3.14159265358979323846f;
        _scene->set_perspective(45.0f * pi / 180.0f, aspect, 0.01f, 100.0f);

        set_scene(_scene);
    }

    ~MeshViewerWindow() override = default;

    void load_obj(const std::filesystem::path &path) {
        spdlog::info("Loading OBJ: {}", path.string());

        auto obj = balsa::geometry::triangle_mesh::read_objF(path);
        const auto &pos = obj.position;
        const auto &nrm = obj.normal;

        if (pos.vertices.extent(1) == 0) {
            spdlog::error("OBJ file has no vertices: {}", path.string());
            return;
        }

        spdlog::info("  {} vertices, {} triangles",
                     pos.vertices.extent(1),
                     pos.triangles.extent(1));

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

        // Add a mesh Object to the scene graph
        auto &mesh_obj = _scene->add_mesh(path.filename().string());
        auto *mesh_data = mesh_obj.find_feature<balsa::scene_graph::MeshData>();

        // Convert positions from ColVectors (SOA row-major) to
        // vector<Vec3f> (AOS)
        std::size_t n_verts = V.extent(1);
        {
            std::vector<balsa::scene_graph::Vec3f> positions(n_verts);
            for (std::size_t j = 0; j < n_verts; ++j) {
                positions[j](0) = V(0, j);
                positions[j](1) = V(1, j);
                positions[j](2) = V(2, j);
            }
            mesh_data->set_positions(positions);
        }

        // Convert and set normals if available
        if (nrm.vertices.extent(1) == pos.vertices.extent(1)) {
            std::vector<balsa::scene_graph::Vec3f> normals(n_verts);
            for (std::size_t j = 0; j < n_verts; ++j) {
                normals[j](0) = nrm.vertices(0, j);
                normals[j](1) = nrm.vertices(1, j);
                normals[j](2) = nrm.vertices(2, j);
            }
            mesh_data->set_normals(normals);
            mesh_data->render_state().normal_source = vk_viz::NormalSource::FromAttribute;
        } else {
            mesh_data->render_state().normal_source = vk_viz::NormalSource::None;
            mesh_data->render_state().shading = vk_viz::ShadingModel::Flat;
        }

        // Convert triangle indices (size_t -> uint32_t)
        if (pos.triangles.extent(1) > 0) {
            std::size_t n_tris = pos.triangles.extent(1);
            std::vector<uint32_t> tri_indices(n_tris * 3);
            for (std::size_t j = 0; j < n_tris; ++j) {
                tri_indices[j * 3 + 0] = static_cast<uint32_t>(pos.triangles(0, j));
                tri_indices[j * 3 + 1] = static_cast<uint32_t>(pos.triangles(1, j));
                tri_indices[j * 3 + 2] = static_cast<uint32_t>(pos.triangles(2, j));
            }
            mesh_data->set_triangle_indices(tri_indices);
        }

        // Convert edge indices
        if (pos.edges.extent(1) > 0) {
            std::size_t n_edges = pos.edges.extent(1);
            std::vector<uint32_t> edge_indices(n_edges * 2);
            for (std::size_t j = 0; j < n_edges; ++j) {
                edge_indices[j * 2 + 0] = static_cast<uint32_t>(pos.edges(0, j));
                edge_indices[j * 2 + 1] = static_cast<uint32_t>(pos.edges(1, j));
            }
            mesh_data->set_edge_indices(edge_indices);
        }

        // If the mesh has edges but no triangles, default to wireframe
        // so that edge-only meshes are visible immediately.
        if (!mesh_data->has_triangle_indices() && mesh_data->has_edge_indices()) {
            mesh_data->render_state().layers.solid.enabled = false;
            mesh_data->render_state().layers.wireframe.enabled = true;
        }

        spdlog::info("  Mesh '{}' added: {} verts, {} tris, {} edges",
                     mesh_obj.name,
                     mesh_data->vertex_count(),
                     mesh_data->triangle_count(),
                     mesh_data->edge_count());
    }

  protected:
    // Gate mouse events: if ImGui wants the mouse, don't forward to the
    // orbit camera controller.
    void dispatch_mouse(const viz::MouseEvent &e) override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
            return;
        }
        viz::glfw::vulkan::Window::dispatch_mouse(e);
    }

    // Gate keyboard events similarly.
    void dispatch_key(const viz::KeyEvent &e) override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }
        viz::glfw::vulkan::Window::dispatch_key(e);
    }

  private:
    std::shared_ptr<MeshViewerScene> _scene;
    std::shared_ptr<vk_viz::OrbitCameraController> _camera;
};

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);

    // ── CLI argument parsing ─────────────────────────────────────────
    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App app{ "Balsa Mesh Viewer (GLFW + Vulkan + ImGui)", "mesh_viewer_glfw" };

    app.add_option("input", input_path, "OBJ file to load")
      ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);
#else
    // Fallback: treat first argument as input file
    if (argc > 1) {
        input_path = argv[1];
    }
#endif

    glfwInit();

    try {
        MeshViewerWindow window("Balsa Mesh Viewer (GLFW)", 1024, 768);

        // Load mesh if path provided
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
