#if !defined(BALSA_VISUALIZATION_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_VULKAN_WINDOW_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include "balsa/visualization/vulkan/scene_base.hpp"

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
    void set_scene(std::shared_ptr<SceneBase> scene);

  protected:
    const std::shared_ptr<SceneBase> &scene() const;

    virtual void pre_draw_hook();
    virtual void post_draw_hook();

    virtual void resize(int w, int h) = 0;

    virtual Film &film() = 0;

  protected:
    void rebuild_swapchains();

  private:
    std::shared_ptr<SceneBase> m_scene;
};
}// namespace balsa::visualization::vulkan

#endif
