#include "balsa/visualization/vulkan/BufferUploadBatch.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/one_shot_command.hpp"

#include <stdexcept>
#include <utility>

namespace balsa::visualization::vulkan {

BufferUploadBatch::BufferUploadBatch(Film &film) : _film(film) {
    if (_film._buffer_upload_batch) {
        throw std::logic_error("BufferUploadBatch: batches must not be nested");
    }
    _film._buffer_upload_batch = this;
}

BufferUploadBatch::~BufferUploadBatch() {
    _film._buffer_upload_batch = nullptr;
}

void BufferUploadBatch::add(vk::Buffer destination,
                            VulkanBuffer staging,
                            vk::DeviceSize size) {
    _copies.push_back({destination, std::move(staging), size});
}

void BufferUploadBatch::submit() {
    if (_copies.empty()) return;
    one_shot_command(_film, [&](vk::CommandBuffer command) {
        for (const auto &copy : _copies) {
            command.copyBuffer(copy.staging.buffer(),
                               copy.destination,
                               vk::BufferCopy{0, 0, copy.size});
        }
        const vk::MemoryBarrier barrier{vk::AccessFlagBits::eTransferWrite,
                                        vk::AccessFlagBits::eMemoryRead};
        command.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eAllCommands,
                                {},
                                barrier,
                                {},
                                {});
    });
    _copies.clear();
}

} // namespace balsa::visualization::vulkan
