#include "balsa/visualization/vulkan/DescriptorAllocator.hpp"
#include "balsa/visualization/vulkan/logging.hpp"

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
    vk::DescriptorPool pool;
    try {
        pool = m_device.createDescriptorPool(ci);
    } catch (const vk::SystemError &e) {
        throw std::runtime_error(std::format(
            "DescriptorAllocator: createDescriptorPool failed for pool {} "
            "(capacity {} sets): {}",
            m_pools.size(),
            sets,
            e.what()));
    }
    m_pools.push_back({pool, sets});
    logger()->debug(
        "DescriptorAllocator: created pool {} with capacity {} sets",
        m_pools.size() - 1,
        sets);
}

auto DescriptorAllocator::allocate(vk::DescriptorSetLayout layout)
    -> vk::DescriptorSet {
    vk::DescriptorSetAllocateInfo ai;
    ai.setSetLayouts(layout);
    vk::DescriptorSet set;
    // Newest pools first: older ones only have room again after frees.
    for (auto index = m_pools.size(); index > 0;) {
        auto &pool = m_pools[--index];
        // Avoid Vulkan 1.0's ambiguous OOM result for exhausted pools.
        if (pool.live_sets == pool.capacity) { continue; }
        ai.setDescriptorPool(pool.handle);
        const auto result = m_device.allocateDescriptorSets(&ai, &set);
        if (result == vk::Result::eSuccess) {
            m_owners.emplace(set, index);
            ++pool.live_sets;
            return set;
        }
        if (result != vk::Result::eErrorOutOfPoolMemory
            && result != vk::Result::eErrorFragmentedPool) {
            throw std::runtime_error(
                std::format("DescriptorAllocator: allocateDescriptorSets "
                            "failed in existing "
                            "pool {} ({} / {} live sets): {}",
                            index,
                            pool.live_sets,
                            pool.capacity,
                            vk::to_string(result)));
        }
    }

    add_pool(m_next_pool_sets);
    m_next_pool_sets *= 2;
    auto &pool = m_pools.back();
    ai.setDescriptorPool(pool.handle);
    if (const auto result = m_device.allocateDescriptorSets(&ai, &set);
        result != vk::Result::eSuccess) {
        throw std::runtime_error(std::format(
            "DescriptorAllocator: allocateDescriptorSets failed in fresh "
            "pool {} (0 / {} live sets): {}",
            m_pools.size() - 1,
            pool.capacity,
            vk::to_string(result)));
    }
    m_owners.emplace(set, m_pools.size() - 1);
    ++pool.live_sets;
    return set;
}

void DescriptorAllocator::free(vk::DescriptorSet set) {
    if (!set) { return; }
    const auto owner = m_owners.find(set);
    if (owner == m_owners.end()) { return; }
    auto &pool = m_pools[owner->second];
    m_device.freeDescriptorSets(pool.handle, set);
    --pool.live_sets;
    m_owners.erase(owner);
}

void DescriptorAllocator::release() {
    if (m_device) {
        for (const auto &pool : m_pools) {
            m_device.destroyDescriptorPool(pool.handle);
        }
    }
    m_pools.clear();
    m_owners.clear();
}

} // namespace balsa::visualization::vulkan
