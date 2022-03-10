#if !defined(BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP

#include "balsa/scene_graph/abstract_feature.hpp"
#include "balsa/scene_graph/abstract_transformation.hpp"
#include "balsa/scene_graph//camera.hpp"

namespace balsa::visualization::vulkan {
class Film;
template<scene_graph::concepts::abstract_transformation TransformationType>
class Drawable : public scene_graph::AbstractFeature<typename TransformationType::embedding_traits> {

  public:
    using embedding_traits = typename TransformationType::embedding_traits;
    using camera_type = scene_graph::Camera<TransformationType>;
    using abstract_feature_type = scene_graph::AbstractFeature<embedding_traits>;
    using abstract_object_type = scene_graph::AbstractObject<embedding_traits>;

  private:
    virtual ~Drawable() {}
    // Drawable(const Object::Ptr& obj, const Shader::Ptr& shader);
    virtual void draw(const camera_type &cam, Film &film) = 0;
};
}// namespace balsa::visualization::vulkan
