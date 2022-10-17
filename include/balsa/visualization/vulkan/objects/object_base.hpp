#if !defined(BALSA_VISUALIZATION_VULKAN_OBJECTS_OBJECT_BASE_HPP)
#define BALSA_VISUALIZATION_VULKAN_OBJECTS_OBJECT_BASE_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/vulkan/film.hpp"


namespace balsa::visualization::vulkan::objects {
class ObjectBase {
  public:
      static ObjectBase from_shader(Film& film, const shaders::AbstractShader& shader);
      ObjectBase(Film& film, const vk::Pipeline& pipeline);
    virtual ~ObjectBase();


  private:
};
}// namespace balsa::visualization::vulkan

#endif
