// mesh_viewer_qt.cpp — Qt + Vulkan mesh viewer with native Qt controls
//
// Usage:  mesh_viewer_qt [--input path/to/model.obj]
//         mesh_viewer_qt path/to/model.obj
//
// Embeds a QVulkanWindow in a QMainWindow with a MeshControlsWidget
// dock panel.  Supports loading OBJ files via File > Open or via
// command-line argument.
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

#include <spdlog/spdlog.h>
#include <balsa/qt/spdlog_logger.hpp>

#include <balsa/visualization/qt/vulkan/window.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>
#include <balsa/visualization/qt/mesh_controls_widget.hpp>
#include <balsa/visualization/qt/SceneGraphWidget.hpp>
#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/MeshData.hpp>

#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/geometry/bounding_box.hpp>

#include <zipper/utils/max_coeff.hpp>

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;

// ── MeshViewerMainWindow ────────────────────────────────────────────
//
// QMainWindow that embeds a Qt Vulkan window and a MeshControlsWidget
// dock.  Holds the scene shared_ptr until it's transferred to the
// Vulkan window.

class MeshViewerMainWindow : public QMainWindow {
  public:
    MeshViewerMainWindow(QVulkanInstance *inst, const std::filesystem::path &initial_obj)
      : QMainWindow(nullptr) {
        setWindowTitle("Balsa Mesh Viewer (Qt)");
        resize(1280, 800);

        // ── Create the Vulkan window ─────────────────────────────────
        _vk_window = new viz::qt::vulkan::Window("Balsa Mesh Viewer (Qt)", 900, 800);
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
        _scene->set_perspective(45.0f * pi / 180.0f, 900.0f / 800.0f, 0.01f, 100.0f);

        // ── Create the scene graph outliner dock ─────────────────────
        _outliner = new viz::qt::SceneGraphWidget();
        _outliner->set_root(&_scene->root());

        _outliner_dock = new QDockWidget("Scene Graph", this);
        _outliner_dock->setWidget(_outliner);
        _outliner_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, _outliner_dock);

        // ── Create the mesh properties dock ──────────────────────────
        _controls = new viz::qt::MeshControlsWidget();
        _controls->set_scene(_scene.get());

        _controls_dock = new QDockWidget("Mesh Properties", this);
        _controls_dock->setWidget(_controls);
        _controls_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, _controls_dock);

        // Stack docks vertically: outliner on top, properties below
        // (Blender-style right sidebar with both panels visible)
        splitDockWidget(_outliner_dock, _controls_dock, Qt::Vertical);

        // ── Wire outliner selection → properties panel ───────────────
        connect(_outliner, &viz::qt::SceneGraphWidget::object_selected, _controls, &viz::qt::MeshControlsWidget::set_selected_object);

        // Re-render when controls or outliner change
        connect(_controls, &viz::qt::MeshControlsWidget::scene_changed, _vk_window, [this]() { _vk_window->requestUpdate(); });
        connect(_outliner, &viz::qt::SceneGraphWidget::scene_changed, _vk_window, [this]() { _vk_window->requestUpdate(); });

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
                load_obj(_initial_obj);
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
              this, tr("Open Mesh"), QString(), tr("OBJ Files (*.obj);;All Files (*)"));
            if (!path.isEmpty()) {
                load_obj(std::filesystem::path(path.toStdString()));
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
    }

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
        balsa::Vector3<float> center_pt = (bb.min() + bb.max()) / 2.0f;
        for (::zipper::index_type j = 0; j < V.extent(1); ++j) {
            auto col = V.col(j);
            col = (col - center_pt) / range;
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

        // Refresh the outliner and select the new mesh in the properties panel
        _outliner->refresh();
        _controls->set_selected_object(&mesh_obj);

        // Request a redraw
        _vk_window->requestUpdate();
    }
};

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    // ── CLI argument parsing (before QApplication consumes args) ─────
    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App cli{ "Balsa Mesh Viewer (Qt + Vulkan)", "mesh_viewer_qt" };

    cli.add_option("input", input_path, "OBJ file to load")
      ->check(CLI::ExistingFile);

    // Allow Qt-specific flags (e.g. -platform) to pass through.
    cli.allow_extras(true);
    CLI11_PARSE(cli, argc, argv);
#else
    // Fallback: treat first argument as input file
    if (argc > 1) {
        input_path = argv[1];
    }
#endif

    QApplication app(argc, argv);
    spdlog::set_level(spdlog::level::info);
    balsa::qt::activateSpdlogOutput();

    std::filesystem::path obj_path;
    if (!input_path.empty()) {
        obj_path = input_path;
    }

    // QVulkanInstance must outlive the QVulkanWindow
    QVulkanInstance inst;
    inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
    if (!inst.create()) {
        spdlog::error("Failed to create QVulkanInstance");
        return 1;
    }

    MeshViewerMainWindow main_window(&inst, obj_path);
    main_window.show();

    return app.exec();
}
