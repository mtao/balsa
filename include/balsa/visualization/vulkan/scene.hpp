#if !defined(BALSA_VISUALIZATION_VULKAN_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_SCENE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>

namespace balsa::visualization::vulkan {
namespace objects {
    class Node;
}

class Camera;
class Film;

class Scene {
  public:
    Scene(const std::shared_ptr<objects::Node> &root = nullptr);
    ~Scene();

    void draw(const Camera &cam, Film &film);

    void set_clear_color(float r, float g, float b, float a = 1.f);
    void set_clear_color(int32_t r, int32_t g, int32_t b, int32_t a = std::numeric_limits<int32_t>::max());
    void set_clear_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a = std::numeric_limits<uint32_t>::max());

  private:
    std::shared_ptr<objects::Node> _root;

    VkClearColorValue _clear_color = { 0.f, 0.f, 0.f, 0.f };
    VkClearDepthStencilValue _clear_depth_stencil = { 1.0f, 0 };
    bool _do_clear_color = true;
    bool _do_clear_depth = true;
};
};// namespace balsa::visualization::vulkan

#endif
