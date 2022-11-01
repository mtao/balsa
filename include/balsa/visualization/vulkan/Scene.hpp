
#if !defined(BALSA_VISUALIZATION_VULKAN_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_SCENE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/Camera.hpp"
#include "SceneBase.hpp"

namespace balsa {
namespace visualization::vulkan {

    class Film;
    class DrawableGroup;

    template<scene_graph::concepts::AbstractTransformation TransformationType>
    class Scene : public SceneBase {
      public:
        using EmbeddingTraits = typename TransformationType::EmbeddingTraits;
        using camera_type = scene_graph::Camera<TransformationType>;
        // virtual void draw(const camera_type &cam, Film &film) { return draw(film); }
        virtual void draw(const camera_type &, Film &film) { return draw(film); }
        virtual void draw(Film &film) = 0;
        virtual ~Scene() = default;
    };
}// namespace visualization::vulkan
}// namespace balsa
#endif
