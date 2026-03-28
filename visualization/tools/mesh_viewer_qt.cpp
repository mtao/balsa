// mesh_viewer_qt.cpp — Qt + Vulkan mesh viewer with native Qt controls
//
// Usage:  mesh_viewer_qt [--input path/to/model.{obj,msh}]
//         mesh_viewer_qt path/to/model.{obj,msh}
//
// Loads an OBJ or MSH mesh via quiver I/O, renders it with Phong
// shading, and provides Qt dock panels for scene graph navigation
// and mesh property editing.
//
// Camera: left-drag orbit, right-drag pan, scroll zoom.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#if BALSA_HAS_CLI11
#include <CLI/CLI.hpp>
#endif
#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenuBar>
#include <QTimer>
#include <QVulkanInstance>
#include <QWidget>

#include <balsa/qt/spdlog_logger.hpp>
#include <spdlog/spdlog.h>

// Qt defines 'emit' as a macro, which conflicts with TBB's
// tbb::profiling::emit().  Undefine it before pulling in headers
// that transitively include TBB (via quiver), then restore it.
#undef emit

#include <balsa/scene_graph/MeshData.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/visualization/qt/SceneGraphWidget.hpp>
#include <balsa/visualization/qt/mesh_controls_widget.hpp>
#include <balsa/visualization/qt/vulkan/window.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>

#include <quiver/attributes/StoredAttribute.hpp>
#include <quiver/io/mesh_io.hpp>

// Restore Qt's emit macro (expands to nothing, but needed for readability).
#define emit

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;
namespace sg = balsa::scene_graph;

// ── MeshViewerMainWindow ────────────────────────────────────────────
//
// QMainWindow that embeds a Qt Vulkan window and a MeshControlsWidget
// dock.  Holds the scene shared_ptr until it's transferred to the
// Vulkan window.

class MeshViewerMainWindow : public QMainWindow {
  public:
    MeshViewerMainWindow(QVulkanInstance *inst,
                         const std::filesystem::path &initial_obj)
      : QMainWindow(nullptr) {
        setWindowTitle("Balsa Mesh Viewer (Qt)");
        resize(1280, 800);

        // ── Create the Vulkan window ─────────────────────────────────
        _vk_window =
            new viz::qt::vulkan::Window("Balsa Mesh Viewer (Qt)", 900, 800);
        _vk_window->setVulkanInstance(inst);

        // Wrap QVulkanWindow in a widget for embedding
        QWidget *wrapper = QWidget::createWindowContainer(_vk_window, this);
        setCentralWidget(wrapper);

        // ── Create the scene ─────────────────────────────────────────
        _scene = std::make_shared<vk_viz::MeshScene>();

        // ── Set up orbit camera ──────────────────────────────────────
        _camera = std::make_shared<vk_viz::OrbitCameraController>(_scene.get());
        _camera->set_distance(2.5f);
        _camera->set_angles(0.4f, 0.3f);
        _vk_window->set_input_handler(_camera);

        // Initial projection
        constexpr float pi = 3.14159265358979323846f;
        _scene->set_perspective(
            45.0f * pi / 180.0f, 900.0f / 800.0f, 0.01f, 100.0f);

        // ── Create the scene graph outliner dock ─────────────────────
        _outliner = new viz::qt::SceneGraphWidget();
        _outliner->set_root(&_scene->root());

        _outliner_dock = new QDockWidget("Scene Graph", this);
        _outliner_dock->setWidget(_outliner);
        _outliner_dock->setAllowedAreas(Qt::LeftDockWidgetArea
                                        | Qt::RightDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, _outliner_dock);

        // ── Create the mesh properties dock ──────────────────────────
        _controls = new viz::qt::MeshControlsWidget();
        _controls->set_scene(_scene.get());

        _controls_dock = new QDockWidget("Mesh Properties", this);
        _controls_dock->setWidget(_controls);
        _controls_dock->setAllowedAreas(Qt::LeftDockWidgetArea
                                        | Qt::RightDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, _controls_dock);

        // Stack docks vertically: outliner on top, properties below
        // (Blender-style right sidebar with both panels visible)
        splitDockWidget(_outliner_dock, _controls_dock, Qt::Vertical);

        // ── Wire outliner selection → properties panel ───────────────
        connect(_outliner,
                &viz::qt::SceneGraphWidget::object_selected,
                _controls,
                &viz::qt::MeshControlsWidget::set_selected_object);

        // Re-render when controls or outliner change
        connect(_controls,
                &viz::qt::MeshControlsWidget::scene_changed,
                _vk_window,
                [this]() { _vk_window->requestUpdate(); });
        connect(_outliner,
                &viz::qt::SceneGraphWidget::scene_changed,
                _vk_window,
                [this]() { _vk_window->requestUpdate(); });

        // Camera activation from the outliner
        connect(_outliner,
                &viz::qt::SceneGraphWidget::camera_activated,
                this,
                [this](balsa::scene_graph::Object *cam_obj) {
                    _scene->set_active_camera(cam_obj);
                    _vk_window->requestUpdate();
                });

        // ── Create menus ─────────────────────────────────────────────
        create_menus();

        // ── Set the scene on the window ──────────────────────────────
        _vk_window->set_scene(_scene);

        // ── Queue initial mesh load (must happen after Film init) ────
        _initial_obj = initial_obj;
    }

    ~MeshViewerMainWindow() override = default;

    // Called once the Vulkan film is ready (e.g., from a QTimer::singleShot
    // or after the window is shown and rendered the first frame).
    // For simplicity, we do a deferred load via showEvent.
    void showEvent(QShowEvent *event) override {
        QMainWindow::showEvent(event);

        // Deferred load: by the time the window is shown, Qt's Vulkan
        // infrastructure should be initializing.  We use a single-shot
        // timer to give it one more event-loop pass.
        if (!_initial_obj.empty() && !_loaded) {
            QTimer::singleShot(100, this, [this]() {
                load_mesh(_initial_obj);
                _loaded = true;
            });
        }
    }

  private:
    viz::qt::vulkan::Window *_vk_window = nullptr;
    std::shared_ptr<vk_viz::MeshScene> _scene;
    std::shared_ptr<vk_viz::OrbitCameraController> _camera;
    viz::qt::MeshControlsWidget *_controls = nullptr;
    viz::qt::SceneGraphWidget *_outliner = nullptr;
    QDockWidget *_outliner_dock = nullptr;
    QDockWidget *_controls_dock = nullptr;
    std::filesystem::path _initial_obj;
    bool _loaded = false;

    void create_menus() {
        QMenu *file_menu = menuBar()->addMenu(tr("&File"));

        QAction *open_action = file_menu->addAction(tr("&Open..."));
        open_action->setShortcuts(QKeySequence::Open);
        connect(open_action, &QAction::triggered, this, [this]() {
            QString path = QFileDialog::getOpenFileName(
                this,
                tr("Open Mesh"),
                QString(),
                tr("Mesh Files (*.obj *.msh);;OBJ Files (*.obj);;MSH Files "
                   "(*.msh);;All Files (*)"));
            if (!path.isEmpty()) {
                load_mesh(std::filesystem::path(path.toStdString()));
            }
        });

        file_menu->addSeparator();

        QAction *quit_action = file_menu->addAction(tr("&Quit"));
        quit_action->setShortcuts(QKeySequence::Quit);
        connect(quit_action, &QAction::triggered, this, &QMainWindow::close);

        // ── View menu ────────────────────────────────────────────────
        QMenu *view_menu = menuBar()->addMenu(tr("&View"));
        view_menu->addAction(_outliner_dock->toggleViewAction());
        view_menu->addAction(_controls_dock->toggleViewAction());

        // Scene Lighting is part of the Mesh Properties dock, so we
        // just ensure the properties dock is visible when the user
        // wants lighting controls.  (The Scene Lighting group is
        // always present inside MeshControlsWidget.)
    }

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

        // Add a mesh Object to the scene graph.
        auto &mesh_obj = _scene->add_mesh(path.filename().string());
        auto *mesh_data = mesh_obj.find_feature<sg::MeshData>();

        // set_mesh() enumerates attributes, auto-assigns roles by
        // convention name (vertex_positions -> position, vertex_normals
        // -> normal), builds skeletons, and extracts topology indices.
        mesh_data->set_mesh(mesh);

        // Apply constraints: auto-selects shading/normal_source based
        // on whether the mesh actually has normal data.
        mesh_data->render_state().constrain(mesh_data->has_normals());

        // Compute bounding box from the position binding and set Object
        // transform to center and normalize the mesh.  The source data
        // stays pristine -- normalization is applied via the Object's
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

        // If the mesh has edges but no triangles, default to wireframe
        // so that edge-only meshes are visible immediately.
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

        // Refresh the outliner and select the new mesh (this also
        // updates the properties panel via the object_selected signal).
        _outliner->refresh();
        _outliner->select_object(&mesh_obj);

        // Request a redraw
        _vk_window->requestUpdate();
    }
};

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    // ── CLI argument parsing (before QApplication consumes args) ─────
    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App cli{"Balsa Mesh Viewer (Qt + Vulkan)", "mesh_viewer_qt"};

    cli.add_option("input", input_path, "Mesh file to load (OBJ, MSH)")
        ->check(CLI::ExistingFile);

    // Allow Qt-specific flags (e.g. -platform) to pass through.
    cli.allow_extras(true);
    CLI11_PARSE(cli, argc, argv);
#else
    // Fallback: treat first argument as input file
    if (argc > 1) { input_path = argv[1]; }
#endif

    QApplication app(argc, argv);
    spdlog::set_level(spdlog::level::info);
    balsa::qt::activateSpdlogOutput();

    std::filesystem::path obj_path;
    if (!input_path.empty()) { obj_path = input_path; }

    // QVulkanInstance must outlive the QVulkanWindow
    QVulkanInstance inst;
    inst.setLayers({"VK_LAYER_KHRONOS_validation"});
    if (!inst.create()) {
        spdlog::error("Failed to create QVulkanInstance");
        return 1;
    }

    MeshViewerMainWindow main_window(&inst, obj_path);
    main_window.show();

    return app.exec();
}
