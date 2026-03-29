// mesh_viewer_glfw.cpp — GLFW + Vulkan mesh viewer with ImGui controls
//
// Usage:  mesh_viewer_glfw [--input path/to/model.obj]
//         mesh_viewer_glfw path/to/model.obj
//         mesh_viewer_glfw path/to/model.msh
//
// Loads an OBJ or MSH mesh via quiver I/O, renders it with Phong
// shading, and provides an ImGui panel for tweaking render state
// (shading model, color source, colormaps, lighting, wireframe, etc.)
// and selecting which attribute is bound to each visualization role.
//
// BVH bounding volume visualization is available via right-click
// context menu on mesh objects in the scene graph panel.
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

#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/visualization/glfw/vulkan/window.hpp>
#include <balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp>
#include <balsa/visualization/vulkan/imgui_integration.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>

#include <quiver/attributes/StoredAttribute.hpp>
#include <quiver/io/mesh_io.hpp>

#if BALSA_HAS_LUA
#include <balsa/geometry/lua/bindings.hpp>
#include <balsa/lua/lua_repl.hpp>
#include <balsa/visualization/vulkan/imgui/lua_repl_panel.hpp>
#include <quiver/lua/bindings.hpp>
#include <sol/sol.hpp>
#endif

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;
namespace sg = balsa::scene_graph;

// ── MeshViewerScene ─────────────────────────────────────────────────
//
// A MeshScene subclass that adds an ImGui overlay with mesh controls
// and a main menu bar.  BVH bounding volume visualization is handled
// per-mesh via BVHData features in the scene graph — the base class
// MeshScene::begin_render_pass() applies deferred updates automatically.

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

#if BALSA_HAS_LUA
    auto set_repl(balsa::lua::LuaRepl *repl) -> void { _repl = repl; }
#endif

    void draw(vk_viz::Film &film) override {
        // Draw mesh geometry first
        MeshScene::draw(film);

        // Then draw ImGui overlay on top
        if (_imgui.is_initialized()) {
            _imgui.new_frame();
            draw_main_menu_bar();
            draw_open_file_dialog();
            vk_viz::imgui::draw_mesh_controls(*this, _panel_state);
#if BALSA_HAS_LUA
            if (_repl) {
                vk_viz::imgui::draw_lua_repl(_repl_panel_state, *_repl);
            }
#endif
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
#if BALSA_HAS_LUA
    balsa::lua::LuaRepl *_repl = nullptr;
    vk_viz::imgui::LuaReplPanelState _repl_panel_state;
#endif

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
                ImGui::MenuItem("Scene Lighting",
                                nullptr,
                                &_panel_state.show_lighting_panel);
#if BALSA_HAS_LUA
                if (_repl) {
                    ImGui::MenuItem(
                        "Lua REPL", nullptr, &_repl_panel_state.show_panel);
                }
#endif
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

#if BALSA_HAS_LUA
        // ── Lua REPL setup ──────────────────────────────────────────
        _repl = std::make_unique<balsa::lua::LuaRepl>();

        // Load quiver and geometry bindings into the Lua state.
        quiver::lua::load_bindings(_repl->lua_state());
        balsa::geometry::lua::load_bindings(_repl->lua_state());

        // Post-execute callback: rediscover attributes on all scene
        // meshes so that Lua-created attributes appear in the UI.
        _repl->set_post_execute_callback([this]() {
            // Recursive traversal of scene graph to refresh all MeshData.
            auto visit = [](auto &self, sg::Object &obj) -> void {
                if (auto *md = obj.find_feature<sg::MeshData>()) {
                    md->rediscover_attributes();
                }
                for (auto &child : obj.children()) { self(self, *child); }
            };
            visit(visit, _scene->root());
        });

        _scene->set_repl(_repl.get());
#endif

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
        constexpr float pi = 3.14159265358979323846f;
        _scene->set_perspective(45.0f * pi / 180.0f, aspect, 0.01f, 100.0f);

        set_scene(_scene);
    }

    ~MeshViewerWindow() override = default;

    void load_mesh(const std::filesystem::path &path) {
        spdlog::info("Loading mesh: {}", path.string());

        // Use quiver's format-dispatching reader (OBJ, MSH, ...).
        auto result = quiver::io::read_mesh(path);
        if (!result) {
            spdlog::error("Failed to load mesh: {}", path.string());
            return;
        }
        auto mesh = std::move(*result);

        spdlog::info("  Mesh dimension: {}",
                     static_cast<int>(mesh->dimension()));

        // Compute bounding box from the position attribute for Object
        // transform normalization.  Position attribute types may be
        // array<double,3>, array<double,2>, array<float,3>, etc.
        // We iterate the discovered attributes after set_mesh() to find
        // the position binding and compute the AABB.

        // Add a mesh Object to the scene graph.
        auto &mesh_obj = _scene->add_mesh(path.filename().string());
        auto *mesh_data = mesh_obj.find_feature<sg::MeshData>();

        // set_mesh() enumerates attributes, auto-assigns roles by
        // convention name (vertex_positions → position, vertex_normals
        // → normal), builds skeletons, and extracts topology indices.
        mesh_data->set_mesh(mesh);

        // Apply constraints: auto-selects shading/normal_source based
        // on whether the mesh actually has normal data.
        mesh_data->render_state().constrain(mesh_data->has_normals());

        // Compute bounding box from the position binding and set Object
        // transform to center and normalize the mesh.  The source data
        // stays pristine — normalization is applied via the Object's
        // local transform.
        if (mesh_data->has_positions()) {
            const auto &pos_bind = mesh_data->position_binding();
            const auto *fdata = static_cast<const float *>(pos_bind.raw_data());
            std::size_t n = pos_bind.size();
            uint8_t comp = pos_bind.component_count;

            if (fdata && n > 0 && comp >= 1) {
                // Compute AABB in up to 3 dimensions.
                float bbmin[3] = {1e30f, 1e30f, 1e30f};
                float bbmax[3] = {-1e30f, -1e30f, -1e30f};
                for (std::size_t i = 0; i < n; ++i) {
                    for (uint8_t c = 0; c < std::min(comp, uint8_t(3)); ++c) {
                        float v = fdata[i * comp + c];
                        bbmin[c] = std::min(bbmin[c], v);
                        bbmax[c] = std::max(bbmax[c], v);
                    }
                }
                // For missing dimensions, leave at 0.
                for (uint8_t c = comp; c < 3; ++c) {
                    bbmin[c] = 0.0f;
                    bbmax[c] = 0.0f;
                }

                // Compute center and range.
                sg::Vec3f center;
                float range = 0.0f;
                for (int c = 0; c < 3; ++c) {
                    center(c) = (bbmin[c] + bbmax[c]) * 0.5f;
                    range = std::max(range, bbmax[c] - bbmin[c]);
                }
                if (range < 1e-8f) range = 1.0f;

                // Apply normalization via Object transform.
                sg::Vec3f neg_center;
                neg_center(0) = -center(0);
                neg_center(1) = -center(1);
                neg_center(2) = -center(2);
                mesh_obj.set_translation(neg_center);
                sg::Vec3f uniform_scale;
                float inv_range = 1.0f / range;
                uniform_scale(0) = inv_range;
                uniform_scale(1) = inv_range;
                uniform_scale(2) = inv_range;
                mesh_obj.set_scale_factors(uniform_scale);
            }
        }

        // If the mesh has edges but no triangles, default to wireframe.
        if (!mesh_data->has_triangle_indices()
            && mesh_data->has_edge_indices()) {
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
#if BALSA_HAS_LUA
    std::unique_ptr<balsa::lua::LuaRepl> _repl;
#endif
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
