#pragma once
#include <functional>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

class Film;

// Records commands with `record` into a fresh command buffer, submits it to
// the graphics queue and waits for that submission (not other queued work)
// to finish.  For synchronous work outside the frame loop, e.g. start-up
// uploads.
auto one_shot_command(Film &film, const std::function<void(vk::CommandBuffer)> &record)
    -> void;

} // namespace balsa::visualization::vulkan
