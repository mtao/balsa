#if !defined(BALSA_VISUALIZATION_VULKAN_OBJECTS_NODE_HPP)
#define BALSA_VISUALIZATION_VULKAN_OBJECTS_NODE_HPP
#include <memory>
#include <vector>

namespace balsa::visualization::vulkan {
class Camera;
class Film;
}// namespace balsa::visualization::vulkan
namespace balsa::visualization::vulkan::objects {
struct Node : public std::enable_shared_from_this<Node> {
    using Ptr = std::shared_ptr<Node>;
    virtual ~Node();

    virtual void draw(const Camera &cam, Film &film) const;
    void draw_children(const Camera &cam, Film &film) const;

    private:
    std::vector<Node> _children;
};
}// namespace balsa::visualization::vulkan::objects

#endif
