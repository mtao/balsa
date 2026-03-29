// image_viewer_glfw.cpp — GLFW + Vulkan image viewer with ImGui controls
//
// Usage:  image_viewer_glfw [--input path/to/image.ppm]
//         image_viewer_glfw path/to/image.ppm
//
// Loads a PPM image and displays it with tone-mapping controls
// (exposure, gamma, channel isolation) via an ImGui panel.
//
// Navigation: scroll to zoom, middle-drag to pan.
// The image is rendered as a fullscreen textured triangle with
// orthographic projection.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#if BALSA_HAS_CLI11
#include <CLI/CLI.hpp>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <balsa/visualization/glfw/vulkan/window.hpp>
#include <balsa/visualization/image_io.hpp>
#include <balsa/visualization/vulkan/image_scene.hpp>
#include <balsa/visualization/vulkan/imgui/image_controls_panel.hpp>
#include <balsa/visualization/vulkan/imgui_integration.hpp>

namespace viz = balsa::visualization;
namespace vk_viz = viz::vulkan;

// ── ImageViewerScene ────────────────────────────────────────────────
//
// An ImageScene subclass that adds an ImGui overlay with image display
// controls and a main menu bar.

class ImageViewerScene : public vk_viz::ImageScene {
  public:
    ImageViewerScene() {
        // Dark background for image viewing.
        set_clear_color(0.1f, 0.1f, 0.1f, 1.0f);
    }
    ~ImageViewerScene() override = default;

    auto init_imgui(vk_viz::Film &film, GLFWwindow *glfw_window) -> void {
        _imgui.init(film, glfw_window);
    }

    using OpenFileCallback = std::function<void(const std::filesystem::path &)>;
    auto set_open_file_callback(OpenFileCallback cb) -> void {
        _open_file_cb = std::move(cb);
    }

    auto draw(vk_viz::Film &film) -> void override {
        // Draw the image first.
        ImageScene::draw(film);

        // Then draw ImGui overlay on top.
        if (_imgui.is_initialized()) {
            _imgui.new_frame();
            draw_main_menu_bar();
            draw_open_file_dialog();
            vk_viz::imgui::draw_image_controls(*this, _panel_state);
            _imgui.render(film);
        }
    }

    auto release_vulkan_resources() -> void override {
        _imgui.shutdown();
        ImageScene::release_vulkan_resources();
    }

  private:
    vk_viz::ImGuiIntegration _imgui;
    vk_viz::imgui::ImagePanelState _panel_state;
    OpenFileCallback _open_file_cb;

    bool _show_open_dialog = false;
    char _path_buf[1024] = {};
    std::string _open_error;

    auto draw_main_menu_bar() -> void {
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
                    "Image Controls", nullptr, &_panel_state.show_controls);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    auto draw_open_file_dialog() -> void {
        if (!_show_open_dialog) return;

        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Open Image File", &_show_open_dialog)) {
            ImGui::Text("Enter PPM file path:");
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

// ── ImageViewerWindow ───────────────────────────────────────────────
//
// Subclass of glfw::vulkan::Window that:
//   1. Overrides dispatch_mouse / dispatch_key to let ImGui consume
//      events before they reach the zoom/pan handler.
//   2. Owns the ImageViewerScene and provides load_image().
//   3. Handles scroll-to-zoom and middle-drag-to-pan.

class ImageViewerWindow : public viz::glfw::vulkan::Window {
  public:
    ImageViewerWindow(const std::string_view &title, int width, int height)
      : viz::glfw::vulkan::Window(title, width, height) {
        _scene = std::make_shared<ImageViewerScene>();

        // Initialize ImGui.
        _scene->init_imgui(film(), glfw_window());

        // Wire up the "Open" dialog callback.
        _scene->set_open_file_callback(
            [this](const std::filesystem::path &p) { load_image(p); });

        set_scene(_scene);
    }

    ~ImageViewerWindow() override = default;

    auto load_image(const std::filesystem::path &path) -> void {
        spdlog::info("Loading image: {}", path.string());

        auto result = viz::load_ppm(path.string());
        if (!result) {
            spdlog::error("Failed to load image: {} ({})",
                          path.string(),
                          viz::error_string(result.error()));
            return;
        }

        auto &img = *result;
        spdlog::info("  Image size: {} x {}", img.width, img.height);

        _scene->set_image_rgba8(
            img.width, img.height, std::span<const uint8_t>(img.pixels));
        _scene->fit_to_window();
    }

  protected:
    auto dispatch_mouse(const viz::MouseEvent &e) -> void override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        // Scroll to zoom.
        if (e.type == viz::MouseEvent::Type::Scroll) {
            float zoom = _scene->zoom();
            zoom *= (e.scroll_y > 0) ? 1.1f : (1.0f / 1.1f);
            _scene->set_zoom(zoom);
            return;
        }

        // Middle-drag to pan.
        if (e.type == viz::MouseEvent::Type::Move && _middle_dragging) {
            auto fb = framebuffer_size();
            float dx = static_cast<float>(e.x - _last_x)
                       / static_cast<float>(fb[0]) * 2.0f;
            float dy = static_cast<float>(e.y - _last_y)
                       / static_cast<float>(fb[1]) * 2.0f;
            _scene->set_pan(_scene->pan_x() + dx, _scene->pan_y() - dy);
            _last_x = e.x;
            _last_y = e.y;
            return;
        }

        if (e.type == viz::MouseEvent::Type::Press
            && e.button == 2) { // 2 = middle button
            _middle_dragging = true;
            _last_x = e.x;
            _last_y = e.y;
            return;
        }

        if (e.type == viz::MouseEvent::Type::Release
            && e.button == 2) { // 2 = middle button
            _middle_dragging = false;
            return;
        }

        viz::glfw::vulkan::Window::dispatch_mouse(e);
    }

    auto dispatch_key(const viz::KeyEvent &e) -> void override {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
            return;
        }
        viz::glfw::vulkan::Window::dispatch_key(e);
    }

  private:
    std::shared_ptr<ImageViewerScene> _scene;
    bool _middle_dragging = false;
    double _last_x = 0.0;
    double _last_y = 0.0;
};

// ── main ─────────────────────────────────────────────────────────────

auto main(int argc, char *argv[]) -> int {
    spdlog::set_level(spdlog::level::info);

    std::string input_path;
#if BALSA_HAS_CLI11
    CLI::App app{"Balsa Image Viewer (GLFW + Vulkan + ImGui)",
                 "image_viewer_glfw"};

    app.add_option("input", input_path, "PPM image file to load")
        ->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);
#else
    if (argc > 1) { input_path = argv[1]; }
#endif

    glfwInit();

    try {
        ImageViewerWindow window("Balsa Image Viewer (GLFW)", 1280, 960);

        if (!input_path.empty()) {
            window.load_image(std::filesystem::path(input_path));
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
