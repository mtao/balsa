
#if !defined(BALSA_VISUALIZATION_VULKAN_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_SCENE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/camera.hpp"
#include "scene_base.hpp"

namespace balsa {
namespace visualization::vulkan {

    class Film;
    class DrawableGroup;

    template<scene_graph::concepts::abstract_transformation TransformationType>
    class Scene : public SceneBase {
      public:
        using embedding_traits = typename TransformationType::embedding_traits;
        using camera_type = scene_graph::Camera<TransformationType>;
        virtual void draw(const camera_type &cam, Film &film) = 0;
        virtual ~Scene() = default;
    };
}// namespace visualization::vulkan
}// namespace balsa
#endif
