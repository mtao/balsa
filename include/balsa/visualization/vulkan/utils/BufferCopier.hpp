#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_BUFFER_COPIER_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_BUFFER_COPIER_HPP
#include <memory>
#include <vulkan/vulkan.hpp>
#include "../Buffer.hpp"

namespace balsa::visualization::vulkan {
class Film;
namespace utils {


    class BufferCopier {
      public:
        static BufferCopier from_film(Film const &, vk::Fence const &fence = {});
        BufferCopier(vk::Device const &device, vk::CommandPool const &command_pool, vk::Queue const &queue, vk::Fence const &fence = {});
        void operator()(const Buffer &src, Buffer &dst, vk::DeviceSize size, vk::DeviceSize src_offset = 0, vk::DeviceSize dst_offset = 0) const;

        void copy_direct(Buffer &buffer, void *data, vk::DeviceSize size, vk::BufferUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void copy_with_staging_buffer(Buffer &buffer, void *data, vk::DeviceSize size, vk::BufferUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Buffer staged_from_device(Film const&, void *data, vk::DeviceSize size,, vk::DeviceSize dst_offset = 0) const;

      private:
        vk::Device m_device;
        vk::CommandPool m_command_pool;
        vk::Queue m_queue;
        vk::Fence m_fence;
    };
}// namespace utils
}// namespace balsa::visualization::vulkan
#endif
