#if !defined(BALSA_VISUALIZATION_VULKAN_OBJECTS_NODE_HPP)
#define BALSA_VISUALIZATION_VULKAN_OBJECTS_NODE_HPP
#include <memory>

namespace balsa::visualization::vulkan {
class Camera;
class Film;
}// namespace balsa::visualization::vulkan
namespace balsa::visualization::vulkan::objects {
struct Node : public std::enable_shared_from_this<Node> {
    virtual ~Node();

    virtual void draw(const Camera &cam, Film &film) = 0;
};
}// namespace balsa::visualization::vulkan::objects

#endif
