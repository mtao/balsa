#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_HPP
#include <memory>

namespace balsa::visualization::vulkan {
class Buffer : public std::enable_shared_from_this<Buffer> {
    using Ptr = std::shared_ptr<Buffer>;


  private:
    int32_t _id;
};
}// namespace balsa::visualization::vulkan

#endif
