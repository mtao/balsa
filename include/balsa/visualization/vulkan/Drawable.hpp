#if !defined(BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP

#include "balsa/scene_graph/AbstractFeature.hpp"
#include "balsa/scene_graph/AbstractTransformation.hpp"
#include "balsa/scene_graph//Camera.hpp"
#include "balsa/scene_graph//FeatureGroup.hpp"

namespace balsa::visualization::vulkan {
class Film;
// template<scene_graph::concepts::AbstractTransformation TransformationType>
// class Drawable : public scene_graph::AbstractFeature<typename TransformationType::EmbeddingTraits> {
//
//   public:
//     using EmbeddingTraits = typename TransformationType::EmbeddingTraits;
//     using camera_type = scene_graph::Camera<TransformationType>;
//     using AbstractFeature_type = scene_graph::AbstractFeature<EmbeddingTraits>;
//     using AbstractObject_type = scene_graph::AbstractObject<EmbeddingTraits>;
//
//   private:
//     virtual ~Drawable() {}
//     // Drawable(const Object::Ptr& obj, const Shader::Ptr& shader);
//     virtual void draw(const camera_type &cam, Film &film) = 0;
// };


class Drawable {
  public:
    virtual ~Drawable() {}
    virtual void initialize(Film &);
    virtual void begin_render_pass(Film &);
    virtual void end_render_pass(Film &);
    virtual void draw(Film &film) = 0;

    // TODO: surface_image_format requests
    // TODO: surface color space requests
    // TODO: present mode requests
};
}// namespace balsa::visualization::vulkan
#endif
