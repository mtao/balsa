#if !defined(BALSA_VISUALIZATION_GLFW_WINDOW_HPP)
#define BALSA_VISUALIZATION_GLFW_WINDOW_HPP
#include <string_view>

class GLFWwindow;
namespace balsa::visualization::glfw {
class Window {
  public:
    Window(const std::string_view &title, int width = 600, int height = 400);
    virtual ~Window();
    GLFWwindow *window() const { return m_window; }

    static bool is_GLFWwindow_managed(GLFWwindow *);

    virtual void pre_draw_hook();
    virtual void post_draw_hook();
    void main_loop();
    virtual void draw_frame() = 0;

    virtual void set_window_pos(int xpos, int ypos);
    virtual void set_window_size(int width, int height);
    virtual void set_window_close();
    virtual void set_window_refresh();
    virtual void set_window_focus(int focused);
    virtual void set_window_iconify(int iconified);
    virtual void set_window_maximize(int maximized);
    virtual void set_framebuffer_size(int width, int height);
    virtual void set_window_content_scale(float xscale, float yscale);

  private:
    GLFWwindow *m_window = nullptr;

    void initialize_glfw_callbacks();
    static void setWindowPos_callback(GLFWwindow *window, int xpos, int ypos);
    static void setWindowSize_callback(GLFWwindow *window, int width, int height);
    static void setWindowClose_callback(GLFWwindow *window);
    static void setWindowRefresh_callback(GLFWwindow *window);
    static void setWindowFocus_callback(GLFWwindow *window, int focused);
    static void setWindowIconify_callback(GLFWwindow *window, int iconified);
    static void setWindowMaximize_callback(GLFWwindow *window, int maximized);
    static void setFramebufferSize_callback(GLFWwindow *window, int width, int height);
    static void setWindowContentScale_callback(GLFWwindow *window, float xscale, float yscale);
};
}// namespace balsa::visualization::glfw
#endif
