#if !defined(BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_DRAWABLE_HPP

#include "balsa/scene_graph/Drawable.hpp"
#include "balsa/scene_graph/Camera.hpp"

namespace balsa::visualization::vulkan {

class Film;

// ── VulkanDrawable ──────────────────────────────────────────────────
//
// A scene_graph::Drawable that additionally knows how to issue
// Vulkan draw commands.  Subclasses implement draw() to record
// commands into the Film's current command buffer.

class VulkanDrawable : public scene_graph::Drawable {
  public:
    using scene_graph::Drawable::Drawable;// inherit ctor

    virtual void draw(const scene_graph::Camera &cam, Film &film) = 0;
};

}// namespace balsa::visualization::vulkan

#endif
