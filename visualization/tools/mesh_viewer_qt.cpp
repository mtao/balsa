// mesh_viewer_qt.cpp — Qt + Vulkan mesh viewer with native Qt controls
//
// Usage:  mesh_viewer_qt [--input path/to/model.{obj,gltf,glb}]
//         mesh_viewer_qt path/to/model.{obj,gltf,glb}
//
// Loads an OBJ mesh or glTF scene, renders it with Phong
// shading, and provides Qt dock panels for scene graph navigation
// and mesh property editing.
//
// Camera: left-drag orbit, right-drag pan, scroll zoom.

#include <algorithm>
#include <chrono>
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
// that transitively include TBB (via zipper <execution>), then restore it.
#undef emit

#include <balsa/scene_graph/Object.hpp>
#include <balsa/scene_graph/SceneLoader.hpp>
#include <balsa/visualization/normalize_mesh.hpp>
#include <balsa/visualization/qt/ControlsWidget.hpp>
#include <balsa/visualization/qt/SceneGraphWidget.hpp>
#include <balsa/visualization/qt/vulkan/window.hpp>
#include <balsa/visualization/vulkan/BufferAllocator.hpp>
#include <balsa/visualization/vulkan/mesh_render_state.hpp>
#include <balsa/visualization/vulkan/mesh_scene.hpp>
#include <balsa/visualization/vulkan/orbit_camera_controller.hpp>

#include <zipper/transform/common.hpp>

// Restore Qt's emit macro (expands to nothing, but needed for readability).
#define emit

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;
namespace sg = balsa::scene_graph;

namespace balsa::visualization::tools {

namespace {

    // ── MeshViewerMainWindow ────────────────────────────────────────────
    //
    // QMainWindow that embeds a Qt Vulkan window and a ControlsWidget
    // dock.  Holds the scene shared_ptr until it's transferred to the
    // Vulkan window.

    class MeshViewerMainWindow : public QMainWindow {
      public:
        MeshViewerMainWindow(QVulkanInstance *inst,
                             const std::filesystem::path &initial_obj)
          : QMainWindow(nullptr) {
            setWindowTitle("Balsa Mesh Viewer (Qt)");
            resize(1280, 800);

            constexpr static int default_width = 900;
            constexpr static int default_height = 800;

            // ── Create the Vulkan window ─────────────────────────────────
            _vk_window = new viz::qt::vulkan::Window(
                "Balsa Mesh Viewer (Qt)", default_width, default_height);
            _vk_window->setVulkanInstance(inst);

            // Wrap QVulkanWindow in a widget for embedding
            QWidget *wrapper = QWidget::createWindowContainer(_vk_window, this);
            setCentralWidget(wrapper);

            // ── Create the scene ─────────────────────────────────────────
            _scene = std::make_shared<vk_viz::MeshScene>();

            // ── Set up orbit camera ──────────────────────────────────────
            _camera =
                std::make_shared<vk_viz::OrbitCameraController>(_scene.get());
            _camera->set_distance(2.5f);
            _camera->set_angles(0.4f, 0.3f);
            _vk_window->set_input_handler(_camera);

            // Initial projection
            _scene->set_perspective(::zipper::transform::radians(45.0f),
                                    static_cast<float>(default_width)
                                        / static_cast<float>(default_height),
                                    0.01f,
                                    100.0f);

            // ── Create the scene graph outliner dock ─────────────────────
            _outliner = new viz::qt::SceneGraphWidget();
            _outliner->set_root(&_scene->root());

            _outliner_dock = new QDockWidget("Scene Graph", this);
            _outliner_dock->setWidget(_outliner);
            _outliner_dock->setAllowedAreas(Qt::LeftDockWidgetArea
                                            | Qt::RightDockWidgetArea);
            addDockWidget(Qt::RightDockWidgetArea, _outliner_dock);

            // ── Create the properties dock ──────────────────────────
            _controls = new viz::qt::ControlsWidget();
            _controls->set_scene(_scene.get());

            _controls_dock = new QDockWidget("Properties", this);
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
                    &viz::qt::ControlsWidget::set_selected_object);

            // Re-render when controls or outliner change
            connect(_controls,
                    &viz::qt::ControlsWidget::scene_changed,
                    _vk_window,
                    [this] -> void { _vk_window->requestUpdate(); });
            connect(_outliner,
                    &viz::qt::SceneGraphWidget::scene_changed,
                    _vk_window,
                    [this] -> void { _vk_window->requestUpdate(); });

            // Camera activation from the outliner
            connect(_outliner,
                    &viz::qt::SceneGraphWidget::camera_activated,
                    this,
                    [this](balsa::scene_graph::Object *cam_obj) -> void {
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
                QTimer::singleShot(100, this, [this] -> void {
                    load_mesh(_initial_obj);
                    _loaded = true;
                });
            }
        }

      private:
        viz::qt::vulkan::Window *_vk_window = nullptr;
        std::shared_ptr<vk_viz::MeshScene> _scene;
        std::shared_ptr<vk_viz::OrbitCameraController> _camera;
        viz::qt::ControlsWidget *_controls = nullptr;
        viz::qt::SceneGraphWidget *_outliner = nullptr;
        QDockWidget *_outliner_dock = nullptr;
        QDockWidget *_controls_dock = nullptr;
        std::filesystem::path _initial_obj;
        bool _loaded = false;

        void create_menus() {
            QMenu *file_menu = menuBar()->addMenu(tr("&File"));

            QAction *open_action = file_menu->addAction(tr("&Open..."));
            open_action->setShortcuts(QKeySequence::Open);
            connect(open_action, &QAction::triggered, this, [this] -> void {
                QString path = QFileDialog::getOpenFileName(
                    this,
                    tr("Open Mesh"),
                    QString(),
                    tr("Mesh Files (*.obj *.gltf *.glb);;OBJ Files (*.obj);;"
                       "glTF Files (*.gltf *.glb);;All Files (*)"));
                if (!path.isEmpty()) {
                    load_mesh(std::filesystem::path(path.toStdString()));
                }
            });

            file_menu->addSeparator();

            QAction *quit_action = file_menu->addAction(tr("&Quit"));
            quit_action->setShortcuts(QKeySequence::Quit);
            connect(
                quit_action, &QAction::triggered, this, &QMainWindow::close);

            // ── View menu ────────────────────────────────────────────────
            QMenu *view_menu = menuBar()->addMenu(tr("&View"));
            view_menu->addAction(_outliner_dock->toggleViewAction());
            view_menu->addAction(_controls_dock->toggleViewAction());

            // Scene Lighting is part of the Mesh Properties dock, so we
            // just ensure the properties dock is visible when the user
            // wants lighting controls.  (The Scene Lighting group is
            // always present inside ControlsWidget.)
        }

        void load_mesh(const std::filesystem::path &path) {
            spdlog::info("Loading mesh: {}", path.string());
            using Clock = std::chrono::steady_clock;
            const auto start = Clock::now();
            const auto seconds = [](auto duration) {
                return std::chrono::duration<double>(duration).count();
            };
            static const auto loader = balsa::scene_graph::SceneLoader::defaults();
            auto loaded = loader.load(path);
            if (!loaded) {
                spdlog::error("Failed to load mesh: {}", loaded.error());
                return;
            }
            const auto decoded = Clock::now();
            spdlog::info(
                "  CPU scene load (parse, codecs, conversion): {:.3f}s",
                seconds(decoded - start));
            auto &object = _scene->add_object(std::move(*loaded));
            const auto initialized = Clock::now();
            spdlog::info("  Initialized '{}' in {:.3f}s",
                         object.name,
                         seconds(initialized - decoded));
            const auto memory =
                _vk_window->film().buffer_allocator()->statistics();
            spdlog::info(
                "  Buffers: {} suballocations in {} memory blocks ({:.1f} MiB "
                "reserved)",
                memory.allocations,
                memory.memory_blocks,
                memory.reserved_bytes / (1024.0 * 1024.0));
            _camera->frame(viz::world_bounding_box(_scene->root()));
            const auto framed = Clock::now();
            spdlog::info("  Framed scene in {:.3f}s",
                         seconds(framed - initialized));

            // Refresh the outliner and select the new mesh (this also
            // updates the properties panel via the object_selected signal).
            _outliner->refresh();
            _outliner->select_object(&object);
            const auto finished = Clock::now();
            spdlog::info("  Updated outliner in {:.3f}s; total load {:.3f}s",
                         seconds(finished - framed),
                         seconds(finished - start));

            // Request a redraw
            _vk_window->requestUpdate();
        }
    };
} // namespace
} // namespace balsa::visualization::tools

// ── main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    // ── CLI argument parsing (before QApplication consumes args) ─────
    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App cli{"Balsa Mesh Viewer (Qt + Vulkan)", "mesh_viewer_qt"};

    cli.add_option("input", input_path, "Mesh file to load (OBJ, glTF, GLB)")
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

    viz::tools::MeshViewerMainWindow main_window(&inst, obj_path);
    main_window.show();

    return app.exec();
}
