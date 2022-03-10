
#if !defined(BALSA_VISUALIZATION_VULKAN_SCENE_BASE_HPP)
#define BALSA_VISUALIZATION_VULKAN_SCENE_BASE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/camera.hpp"

namespace balsa {
namespace visualization::vulkan {

    class Film;
    class DrawableGroup;

    class SceneBase {
      public:
        SceneBase();
        // SceneBase(const  &root = nullptr);
        ~SceneBase();

        void draw_background(Film &film);

        void set_clear_color(float r, float g, float b, float a = 1.f);
        void set_clear_color(int32_t r, int32_t g, int32_t b, int32_t a = std::numeric_limits<int32_t>::max());
        void set_clear_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a = std::numeric_limits<uint32_t>::max());

      private:
        std::shared_ptr<DrawableGroup> _root;

        VkClearColorValue _clear_color = { 0.f, 0.f, 0.f, 0.f };
        VkClearDepthStencilValue _clear_depth_stencil = { 1.0f, 0 };
        bool _do_clear_color = true;
        bool _do_clear_depth = true;
    };
}// namespace visualization::vulkan
}// namespace balsa

#endif
