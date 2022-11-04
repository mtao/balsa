#if !defined(BALSA_VISUALIZATION_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_VULKAN_WINDOW_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include "balsa/visualization/vulkan/Drawable.hpp"

namespace balsa::visualization::vulkan {
class Film;
class SceneBase;

class Window {
  public:
    Window();
    virtual ~Window();


    // run the main loop
    virtual int exec() = 0;
    void draw_frame();
    virtual Drawable *drawable() const = 0;


    virtual void pre_draw_hook();
    virtual void post_draw_hook();

    virtual void resize(int w, int h) = 0;

    virtual Film &film() = 0;
};
}// namespace balsa::visualization::vulkan

#endif
