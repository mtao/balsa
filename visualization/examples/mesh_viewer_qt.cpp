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
#include <balsa/visualization/vulkan/mesh_drawable.hpp>
#include <balsa/visualization/vulkan/mesh_buffers.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>
#include <balsa/visualization/qt/mesh_controls_widget.hpp>

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
        _scene->set_perspective(glm::radians(45.0f), 900.0f / 800.0f, 0.01f, 100.0f);

        // ── Create the controls dock ─────────────────────────────────
        _controls = new viz::qt::MeshControlsWidget();
        _controls->set_scene(_scene.get());

        auto *dock = new QDockWidget("Mesh Controls", this);
        dock->setWidget(_controls);
        dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::RightDockWidgetArea, dock);

        // Re-render when controls change
        connect(_controls, &viz::qt::MeshControlsWidget::scene_changed, _vk_window, [this]() { _vk_window->requestUpdate(); });

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

        auto &film = _vk_window->film();
        auto *drawable = _scene->add_drawable(path.filename().string());

        // Upload positions
        {
            auto sp = V.expression().as_std_span();
            drawable->buffers().upload_positions(
              film,
              std::span<const float>(sp.data(), sp.size()),
              static_cast<uint32_t>(V.extent(1)));
        }

        // Upload normals if available
        if (nrm.vertices.extent(1) == pos.vertices.extent(1)) {
            auto sp = nrm.vertices.expression().as_std_span();
            drawable->buffers().upload_normals(
              film,
              std::span<const float>(sp.data(), sp.size()));
            drawable->render_state.normal_source = vk_viz::NormalSource::FromAttribute;
        } else {
            drawable->render_state.normal_source = vk_viz::NormalSource::None;
            drawable->render_state.shading = vk_viz::ShadingModel::Flat;
        }

        // Upload triangle indices (size_t → uint32 conversion done internally)
        if (pos.triangles.extent(1) > 0) {
            auto sp = pos.triangles.expression().as_std_span();
            drawable->buffers().upload_triangle_indices_from_sizet(
              film,
              std::span<const std::size_t>(sp.data(), sp.size()),
              static_cast<uint32_t>(pos.triangles.extent(1)));
        }

        // Upload edge indices
        if (pos.edges.extent(1) > 0) {
            auto sp = pos.edges.expression().as_std_span();
            drawable->buffers().upload_edge_indices_from_sizet(
              film,
              std::span<const std::size_t>(sp.data(), sp.size()),
              static_cast<uint32_t>(pos.edges.extent(1)));
        }

        spdlog::info("  Drawable '{}' added: {} verts, {} tris, {} edges",
                     drawable->name,
                     drawable->buffers().vertex_count(),
                     drawable->buffers().triangle_count(),
                     drawable->buffers().edge_count());

        // Refresh the controls panel to show the new drawable
        _controls->refresh();

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
