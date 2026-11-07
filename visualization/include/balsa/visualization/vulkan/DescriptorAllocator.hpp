#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

// Allocates descriptor sets from pools that grow on demand.
class DescriptorAllocator {
  public:
    // Descriptors of one type per set.
    struct Descriptors {
        vk::DescriptorType type;
        uint32_t count = 1;
    };

    DescriptorAllocator() = default;
    ~DescriptorAllocator() { release(); }
    DescriptorAllocator(const DescriptorAllocator &) = delete;
    auto operator=(const DescriptorAllocator &) -> DescriptorAllocator & = delete;
    DescriptorAllocator(DescriptorAllocator &&other) noexcept;
    auto operator=(DescriptorAllocator &&other) noexcept -> DescriptorAllocator &;

    // Sets the descriptors per set and the first pool's capacity.
    void init(vk::Device device,
              std::vector<Descriptors> per_set,
              uint32_t initial_sets);

    // Throws std::runtime_error on failure.
    auto allocate(vk::DescriptorSetLayout layout) -> vk::DescriptorSet;
    // Returns `set` to its pool.
    void free(vk::DescriptorSet set);
    // Destroys every pool and set.
    void release();

    [[nodiscard]] auto pool_count() const -> std::size_t { return m_pools.size(); }

  private:
    void add_pool(uint32_t sets);

    vk::Device m_device;
    std::vector<Descriptors> m_per_set;
    uint32_t m_next_pool_sets = 0;
    std::vector<vk::DescriptorPool> m_pools;
    // Pool of each live set.
    std::unordered_map<VkDescriptorSet, vk::DescriptorPool> m_owners;
};

} // namespace balsa::visualization::vulkan
