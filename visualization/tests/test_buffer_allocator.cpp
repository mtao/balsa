#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <balsa/visualization/vulkan/BufferAllocator.hpp>
#include <balsa/visualization/vulkan/BufferUploadBatch.hpp>
#include <balsa/visualization/vulkan/one_shot_command.hpp>

#include "TestFilm.hpp"

#include <array>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace vulkan = balsa::visualization::vulkan;

TEST_CASE("buffer memory is suballocated and reused", "[vulkan][buffer]") {
    const auto api = GENERATE(VK_API_VERSION_1_0, VK_API_VERSION_1_1);
    std::unique_ptr<vulkan::test::TestFilm> film;
    try {
        film = std::make_unique<vulkan::test::TestFilm>(api);
    } catch (const std::exception &e) {
        SKIP("Vulkan unavailable: " << e.what());
    }
    auto allocator = film->buffer_allocator();
    std::vector<vulkan::VulkanBuffer> buffers;
    const std::array<uint32_t, 4> values{1, 2, 3, 4};
    for (int i = 0; i < 2048; ++i) {
        auto &buffer = buffers.emplace_back(
            *film,
            256,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible
                | vk::MemoryPropertyFlagBits::eHostCoherent);
        buffer.upload(values.data(), sizeof(values), 128);
    }
    const auto stats = allocator->statistics();
    CHECK(stats.allocations == buffers.size());
    CHECK(stats.memory_blocks < 16);
    CHECK(stats.used_bytes >= buffers.size() * 256);

    SECTION("moves transfer ownership and release is idempotent") {
        const auto handle = buffers.front().buffer();
        auto moved = std::move(buffers.front());
        CHECK_FALSE(buffers.front());
        CHECK(buffers.front().size() == 0);
        CHECK(moved.buffer() == handle);
        buffers.back() = std::move(moved);
        CHECK_FALSE(moved);
        buffers.back().upload(values.data(), sizeof(values));
        buffers.back().release();
        buffers.back().release();
        CHECK(allocator->statistics().allocations == 2046);
    }
    SECTION("writes are bounded, including overflowing offsets") {
        CHECK_THROWS_AS(
            buffers.front().upload(values.data(), sizeof(values), 250),
            std::runtime_error);
        CHECK_THROWS_AS(
            buffers.front().upload(values.data(),
                                   sizeof(values),
                                   std::numeric_limits<vk::DeviceSize>::max()),
            std::runtime_error);
        CHECK_THROWS_AS(
            vulkan::VulkanBuffer(*film,
                                 0,
                                 vk::BufferUsageFlagBits::eUniformBuffer,
                                 vk::MemoryPropertyFlagBits::eHostVisible),
            std::invalid_argument);
    }
    buffers.clear();
    CHECK(allocator->statistics().allocations == 0);
    for (int i = 0; i < 2048; ++i) {
        buffers.emplace_back(*film,
                             256,
                             vk::BufferUsageFlagBits::eUniformBuffer,
                             vk::MemoryPropertyFlagBits::eHostVisible
                                 | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
    CHECK(allocator->statistics().memory_blocks <= stats.memory_blocks);
}

TEST_CASE("batched buffer uploads survive staging moves and reach the GPU",
          "[vulkan][buffer]") {
    std::unique_ptr<vulkan::test::TestFilm> film;
    try {
        film = std::make_unique<vulkan::test::TestFilm>();
    } catch (const std::exception &e) {
        SKIP("Vulkan unavailable: " << e.what());
    }
    const auto device = film->device();
    std::array<std::array<uint32_t, 4>, 4> values{
        {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}}};
    std::vector<vulkan::VulkanBuffer> destinations;
    for (std::size_t i = 0; i < values.size(); ++i) {
        destinations.emplace_back(*film,
                                  sizeof(values[i]),
                                  vk::BufferUsageFlagBits::eTransferDst
                                      | vk::BufferUsageFlagBits::eTransferSrc,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal);
    }
    {
        vulkan::BufferUploadBatch batch(*film);
        CHECK_THROWS_AS(vulkan::BufferUploadBatch(*film), std::logic_error);
        for (std::size_t i = 0; i < values.size(); ++i) {
            destinations[i].upload_staged(
                *film, values[i].data(), sizeof(values[i]));
        }
        CHECK(film->buffer_allocator()->statistics().allocations == 8);
        batch.submit();
        CHECK(film->buffer_allocator()->statistics().allocations == 4);
        batch.submit();
    }
    CHECK(film->buffer_upload_batch() == nullptr);

    auto readback = device.createBufferUnique(
        vk::BufferCreateInfo{}
            .setSize(sizeof(values))
            .setUsage(vk::BufferUsageFlagBits::eTransferDst));
    const auto requirements = device.getBufferMemoryRequirements(*readback);
    auto memory = device.allocateMemoryUnique(
        vk::MemoryAllocateInfo{}
            .setAllocationSize(requirements.size)
            .setMemoryTypeIndex(film->find_memory_type(
                requirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent)));
    device.bindBufferMemory(*readback, *memory, 0);
    vulkan::one_shot_command(*film, [&](vk::CommandBuffer command) {
        for (std::size_t i = 0; i < values.size(); ++i) {
            command.copyBuffer(
                destinations[i].buffer(),
                *readback,
                vk::BufferCopy{0, i * sizeof(values[i]), sizeof(values[i])});
        }
        const vk::MemoryBarrier barrier{vk::AccessFlagBits::eTransferWrite,
                                        vk::AccessFlagBits::eHostRead};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eHost,
                                {},
                                barrier,
                                {},
                                {});
    });
    const auto *actual = static_cast<const uint32_t *>(
        device.mapMemory(*memory, 0, sizeof(values)));
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = 0; j < values[i].size(); ++j) {
            CHECK(actual[i * values[i].size() + j] == values[i][j]);
        }
    }
    device.unmapMemory(*memory);
    readback.reset();
}
