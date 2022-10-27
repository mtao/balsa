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

    virtual void window_pos(int xpos, int ypos);
    virtual void window_size(int width, int height);
    virtual void window_close();
    virtual void window_refresh();
    virtual void window_focus(int focused);
    virtual void window_iconify(int iconified);
    virtual void window_maximize(int maximized);
    virtual void framebuffer_size(int width, int height);
    virtual void window_content_scale(float xscale, float yscale);

    virtual void key(int key, int scancode, int action, int mods);

    virtual void character(unsigned int codepoint);
    virtual void cursor_position(double xpos, double ypos);
    virtual void mouse_button(int button, int action, int mods);
    virtual void scroll(double xoffset, double yoffset);

  private:
    GLFWwindow *m_window = nullptr;

    void initialize_glfw_callbacks();
    static void windowPos_callback(GLFWwindow *window, int xpos, int ypos);
    static void windowSize_callback(GLFWwindow *window, int width, int height);
    static void windowClose_callback(GLFWwindow *window);
    static void windowRefresh_callback(GLFWwindow *window);
    static void windowFocus_callback(GLFWwindow *window, int focused);
    static void windowIconify_callback(GLFWwindow *window, int iconified);
    static void windowMaximize_callback(GLFWwindow *window, int maximized);
    static void framebufferSize_callback(GLFWwindow *window, int width, int height);
    static void windowContentScale_callback(GLFWwindow *window, float xscale, float yscale);


    static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

    static void character_callback(GLFWwindow *window, unsigned int codepoint);
    static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);

    static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
};
}// namespace balsa::visualization::glfw
#endif
