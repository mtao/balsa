#if !defined(BALSA_VISUALIZATION_SHADERS_FLAT_HPP)
#define BALSA_VISUALIZATION_SHADERS_FLAT_HPP

#include <memory>
#include <limits>
#include <vulkan/vulkan_core.h>
#include "balsa/scene_graph/camera.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"

namespace balsa::visualization::shaders {

class FlatShader : public AbstractShader {
  public:
    FlatShader();
    std::vector<char> vert_spirv() const override final;
    std::vector<char> frag_spirv() const override final;
};
}// namespace balsa::visualization::shaders
#endif
