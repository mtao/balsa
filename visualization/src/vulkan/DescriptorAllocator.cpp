#include "balsa/visualization/vulkan/DescriptorAllocator.hpp"

#include <format>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace balsa::visualization::vulkan {

DescriptorAllocator::DescriptorAllocator(DescriptorAllocator &&other) noexcept
  : m_device(std::exchange(other.m_device, vk::Device{})),
    m_per_set(std::move(other.m_per_set)),
    m_next_pool_sets(std::exchange(other.m_next_pool_sets, 0)),
    m_pools(std::exchange(other.m_pools, {})),
    m_owners(std::exchange(other.m_owners, {})) {}

auto DescriptorAllocator::operator=(DescriptorAllocator &&other) noexcept
    -> DescriptorAllocator & {
    if (this != &other) {
        release();
        m_device = std::exchange(other.m_device, vk::Device{});
        m_per_set = std::move(other.m_per_set);
        m_next_pool_sets = std::exchange(other.m_next_pool_sets, 0);
        m_pools = std::exchange(other.m_pools, {});
        m_owners = std::exchange(other.m_owners, {});
    }
    return *this;
}

void DescriptorAllocator::init(vk::Device device,
                               std::vector<Descriptors> per_set,
                               uint32_t initial_sets) {
    release();
    m_device = device;
    m_per_set = std::move(per_set);
    m_next_pool_sets = initial_sets == 0 ? 1 : initial_sets;
}

void DescriptorAllocator::add_pool(uint32_t sets) {
    const auto sizes =
        m_per_set | std::views::transform([&](const Descriptors &d) {
            return vk::DescriptorPoolSize(d.type, d.count * sets);
        })
        | std::ranges::to<std::vector>();
    vk::DescriptorPoolCreateInfo ci;
    ci.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    ci.setMaxSets(sets);
    ci.setPoolSizes(sizes);
    m_pools.push_back(m_device.createDescriptorPool(ci));
}

auto DescriptorAllocator::allocate(vk::DescriptorSetLayout layout)
    -> vk::DescriptorSet {
    vk::DescriptorSetAllocateInfo ai;
    ai.setSetLayouts(layout);
    vk::DescriptorSet set;
    // Newest pools first: older ones only have room again after frees.
    for (const auto pool : m_pools | std::views::reverse) {
        ai.setDescriptorPool(pool);
        const auto result = m_device.allocateDescriptorSets(&ai, &set);
        if (result == vk::Result::eSuccess) {
            m_owners.emplace(set, pool);
            return set;
        }
        if (result != vk::Result::eErrorOutOfPoolMemory
            && result != vk::Result::eErrorFragmentedPool) {
            throw std::runtime_error(std::format(
                "DescriptorAllocator: allocateDescriptorSets failed: {}",
                vk::to_string(result)));
        }
    }

    add_pool(m_next_pool_sets);
    m_next_pool_sets *= 2;
    ai.setDescriptorPool(m_pools.back());
    if (const auto result = m_device.allocateDescriptorSets(&ai, &set);
        result != vk::Result::eSuccess) {
        throw std::runtime_error(std::format(
            "DescriptorAllocator: allocateDescriptorSets failed on a new "
            "pool: {}",
            vk::to_string(result)));
    }
    m_owners.emplace(set, m_pools.back());
    return set;
}

void DescriptorAllocator::free(vk::DescriptorSet set) {
    if (!set) { return; }
    const auto owner = m_owners.find(set);
    if (owner == m_owners.end()) { return; }
    m_device.freeDescriptorSets(owner->second, set);
    m_owners.erase(owner);
}

void DescriptorAllocator::release() {
    if (m_device) {
        for (const auto pool : m_pools) { m_device.destroyDescriptorPool(pool); }
    }
    m_pools.clear();
    m_owners.clear();
}

} // namespace balsa::visualization::vulkan
