#include "balsa/visualization/vulkan/one_shot_command.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <limits>
#include <stdexcept>

namespace balsa::visualization::vulkan {

auto one_shot_command(Film &film, const std::function<void(vk::CommandBuffer)> &record)
    -> void {
    const auto device = film.device();
    const auto pool = film.graphics_command_pool();
    vk::CommandBufferAllocateInfo allocate;
    allocate.setCommandPool(pool);
    allocate.setLevel(vk::CommandBufferLevel::ePrimary);
    allocate.setCommandBufferCount(1);
    const vk::CommandBuffer cmd = device.allocateCommandBuffers(allocate)[0];
    vk::Fence fence;

    // Frees what was created, also if recording or submission throws.
    struct Cleanup {
        vk::Device device;
        vk::CommandPool pool;
        const vk::CommandBuffer &cmd;
        vk::Fence &fence;
        ~Cleanup() {
            if (fence) device.destroyFence(fence);
            device.freeCommandBuffers(pool, cmd);
        }
    } cleanup{device, pool, cmd, fence};

    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    record(cmd);
    cmd.end();

    // Wait for this submission only, not everything queued (e.g. frames
    // in flight).
    fence = device.createFence({});
    vk::SubmitInfo submit;
    submit.setCommandBuffers(cmd);
    film.graphics_queue().submit(submit, fence);
    if (device.waitForFences(fence, vk::True, std::numeric_limits<uint64_t>::max())
        != vk::Result::eSuccess) {
        throw std::runtime_error("one_shot_command: waiting for the submission failed");
    }
}

} // namespace balsa::visualization::vulkan
