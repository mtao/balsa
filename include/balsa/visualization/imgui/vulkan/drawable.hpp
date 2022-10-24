#if !defined(BALSA_VISUALIZATION_IMGUI_VULKAN_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_IMGUI_VULKAN_DRAWABLE_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/visualization/vulkan/drawable.hpp"

namespace balsa::visualization::imgui::vulkan {


// TODO: find a better drawable name
class Drawable : public visualization::vulkan::Drawable {
  public:
    class Impl;
    using Film = visualization::vulkan::Film;
    Drawable();
    // Drawable(const  &root = nullptr);
    ~Drawable();

    void initialize(Film &film) override;
    void draw(Film &film) override;
    void begin_render_pass(Film &film) override;
    void end_render_pass(Film &film) override;

  private:
    std::unique_ptr<Impl> _impl;
};
}// namespace balsa::visualization::imgui::vulkan

#endif
