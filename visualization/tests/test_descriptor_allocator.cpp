#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <balsa/visualization/vulkan/DescriptorAllocator.hpp>

#include "TestFilm.hpp"

#include <algorithm>
#include <vector>

TEST_CASE("descriptor pools grow and reuse freed sets",
          "[vulkan][descriptor]") {
    const auto api_version = GENERATE(VK_API_VERSION_1_0, VK_API_VERSION_1_1);
    INFO("Vulkan API version: " << api_version);

    std::unique_ptr<balsa::visualization::vulkan::test::TestFilm> film;
    try {
        film = std::make_unique<balsa::visualization::vulkan::test::TestFilm>(
            api_version);
    } catch (const std::exception &e) {
        SKIP("Vulkan unavailable: " << e.what());
    }
    const auto device = film->device();
    const auto binding =
        vk::DescriptorSetLayoutBinding{0,
                                       vk::DescriptorType::eUniformBuffer,
                                       1,
                                       vk::ShaderStageFlagBits::eVertex};
    auto layout = device.createDescriptorSetLayoutUnique(
        vk::DescriptorSetLayoutCreateInfo{}.setBindings(binding));

    balsa::visualization::vulkan::DescriptorAllocator allocator;
    allocator.init(device, {{vk::DescriptorType::eUniformBuffer, 1}}, 1);
    std::vector<vk::DescriptorSet> sets;
    for (int i = 0; i < 65; ++i) {
        const auto set = allocator.allocate(*layout);
        REQUIRE(set);
        CHECK(std::ranges::find(sets, set) == sets.end());
        sets.push_back(set);
    }
    CHECK(allocator.pool_count() == 7);
    const auto pool_count = allocator.pool_count();
    for (const auto set : sets) { allocator.free(set); }
    for (auto &set : sets) {
        set = allocator.allocate(*layout);
        REQUIRE(set);
    }
    CHECK(allocator.pool_count() == pool_count);
    for (const auto set : sets) { allocator.free(set); }
    allocator.release();
    CHECK(allocator.pool_count() == 0);
}
