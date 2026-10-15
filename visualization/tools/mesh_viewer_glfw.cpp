// mesh_viewer_glfw.cpp — GLFW + Vulkan mesh viewer with ImGui controls
//
// Usage:  mesh_viewer_glfw [--input path/to/model.obj]
//         mesh_viewer_glfw path/to/model.obj
//
// Loads an OBJ mesh, renders it with Phong
// shading, and provides an ImGui panel for tweaking render state
// (shading model, color source, colormaps, lighting, wireframe, etc.)
// and selecting which attribute is bound to each visualization role.
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

#include <balsa/scene_graph/BasicShading.hpp>
#include <balsa/scene_graph/MeshInstance.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/visualization/glfw/vulkan/window.hpp>
#include <balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp>
#include <balsa/visualization/vulkan/imgui_integration.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>

#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/visualization/mesh_conversion.hpp>
#include <balsa/visualization/normalize_mesh.hpp>
#include <zipper/transform/common.hpp>

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;
namespace sg = balsa::scene_graph;

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
    void set_open_file_callback(OpenFileCallback cb) {
        _open_file_cb = std::move(cb);
    }

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
  public:
    vk_viz::imgui::MeshPanelState &panel_state() { return _panel_state; }

  private:
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
                if (ImGui::MenuItem("Quit", "Ctrl+Q")) { std::exit(0); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem(
                    "Scene Graph", nullptr, &_panel_state.show_scene_panel);
                ImGui::MenuItem(
                    "Properties", nullptr, &_panel_state.show_property_panel);
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
            bool enter_pressed =
                ImGui::InputText("##path",
                                 _path_buf,
                                 sizeof(_path_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();
            bool load_clicked = ImGui::Button("Load");

            if (enter_pressed || load_clicked) {
                std::filesystem::path p(_path_buf);
                if (std::filesystem::exists(p)) {
                    if (_open_file_cb) { _open_file_cb(p); }
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
        _scene->set_open_file_callback(
            [this](const std::filesystem::path &p) { load_mesh(p); });

        // Set up orbit camera
        _camera = std::make_shared<vk_viz::OrbitCameraController>(_scene.get());
        _camera->set_distance(2.5f);
        _camera->set_angles(0.4f, 0.3f);
        set_input_handler(_camera);

        // Set initial projection
        auto fb = framebuffer_size();
        float aspect =
            (fb[1] > 0) ? static_cast<float>(fb[0]) / static_cast<float>(fb[1])
                        : 1.0f;
        _scene->set_perspective(::zipper::transform::radians(45.0f), aspect, 0.01f, 100.0f);

        set_scene(_scene);
    }

    ~MeshViewerWindow() override = default;

    void load_mesh(const std::filesystem::path &path) {
        spdlog::info("Loading mesh: {}", path.string());

        if (!std::filesystem::exists(path)) {
            spdlog::error("Failed to load mesh: file not found: {}",
                          path.string());
            return;
        }
        auto obj = balsa::geometry::triangle_mesh::read_objF(path);
        if (obj.position.vertices.cols() == 0) {
            spdlog::error("Failed to load mesh: no vertices in {}",
                          path.string());
            return;
        }

        // Add a mesh Object to the scene graph showing the loaded geometry.
        auto &mesh_obj = _scene->add_mesh(path.filename().string(),
                                          viz::to_mesh_geometry(obj));
        const auto *geometry =
            mesh_obj.find_feature<sg::MeshInstance>()->geometry();
        auto *shading = mesh_obj.find_feature<sg::BasicShading>();

        // Apply constraints: auto-selects shading/normal_source based
        // on whether the mesh actually has normal data.
        shading->render_state().constrain(shading->has_normals());

        // Compute bounding box from the position binding and set Object
        // transform to center and normalize the mesh.  The source data
        // stays pristine — normalization is applied via the Object's
        // local transform.
        if (auto xf = viz::unit_box_transform(*shading)) {
            mesh_obj.set_from_transform(*xf);
        }
        _scene->panel_state().selected_object = &mesh_obj;

        // If the mesh has edges but no triangles, default to wireframe.
        if (!geometry->has_triangle_indices()
            && geometry->has_edge_indices()) {
            shading->render_state().layers.solid.enabled = false;
            shading->render_state().layers.wireframe.enabled = true;
        }

        spdlog::info("  Mesh '{}' added: {} verts, {} tris, {} edges",
                     mesh_obj.name,
                     shading->vertex_count(),
                     geometry->triangle_count(),
                     geometry->edge_count());
    }

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
    std::shared_ptr<MeshViewerScene> _scene;
    std::shared_ptr<vk_viz::OrbitCameraController> _camera;
};

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::info);

    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App app{"Balsa Mesh Viewer (GLFW + Vulkan + ImGui)",
                 "mesh_viewer_glfw"};

    app.add_option("input", input_path, "OBJ file to load")
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);
#else
    if (argc > 1) { input_path = argv[1]; }
#endif

    glfwInit();

    try {
        MeshViewerWindow window("Balsa Mesh Viewer (GLFW)", 1280, 960);

        if (!input_path.empty()) {
            window.load_mesh(std::filesystem::path(input_path));
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
