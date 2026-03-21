
#if !defined(BALSA_VISUALIZATION_VULKAN_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_SCENE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/Camera.hpp"
#include "scene_base.hpp"

namespace balsa {
namespace visualization::vulkan {

    class Film;

    // Scene that knows about the scene graph Camera.
    // Subclasses override draw(Film&) to render.
    class Scene : public SceneBase {
      public:
        virtual void draw(Film &film) = 0;
        virtual ~Scene() = default;
    };
}// namespace visualization::vulkan
}// namespace balsa
#endif
