#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_BUFFER_COPIER_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_BUFFER_COPIER_HPP
#include <memory>
#include <vulkan/vulkan.hpp>
#include "../Buffer.hpp"
#include <span>
#include <optional>

namespace balsa::visualization::vulkan {
class Film;
namespace utils {


    class BufferCopier {
      public:
        static BufferCopier from_film(Film const &, vk::Fence const &fence = {});
        BufferCopier(vk::Device const &device, vk::PhysicalDevice const &physical_device, vk::CommandPool const &command_pool, vk::Queue const &queue, vk::Fence const &fence = {});
        void operator()(const Buffer &src, Buffer &dst, vk::DeviceSize size, vk::DeviceSize src_offset = 0, vk::DeviceSize dst_offset = 0) const;


        void copy_direct(Buffer &buffer, void *data, vk::DeviceSize size, vk::BufferUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void copy_with_staging_buffer(Buffer &buffer, void *data, vk::DeviceSize size, vk::BufferUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eDeviceLocal);

        void copy(Buffer &buffer, void *data, vk::DeviceSize data_size, vk::BufferUsageFlags buffer_usage_flags, vk::MemoryPropertyFlags memory_property_flags);

        template<typename DataType>
        void copy_direct(Buffer &buffer, std::span<DataType> data, vk::DeviceSize size, vk::BufferUsageFlags = vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);


        bool staging_buffer_enabled() const;
        void enable_staging_buffer();
        void disable_staging_buffer();
        void create_staging_buffer(vk::DeviceSize);

        vk::Device device() const { return m_staging_buffer.device(); }

        // Buffer staged_from_device(Film const&, void *data, vk::DeviceSize size,, vk::DeviceSize dst_offset = 0) const;

      private:
        vk::CommandPool m_command_pool;
        vk::Queue m_queue;
        vk::Fence m_fence;

        Buffer m_staging_buffer;
        std::optional<vk::DeviceSize> m_staging_buffer_size = {};
    };
}// namespace utils
}// namespace balsa::visualization::vulkan
#endif
