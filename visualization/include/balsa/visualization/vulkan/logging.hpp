#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <spdlog/logger.h>
#include <string_view>

namespace balsa::visualization::vulkan {

// Uses a pre-registered balsa.vulkan logger, or shares the default logger's
// sinks at first use. Configure the default sinks before calling this.
auto logger() -> std::shared_ptr<spdlog::logger>;

// CPU wall-clock profiling; no clock reads when the level is disabled.
// The label must outlive the scope. Frame timings should use trace level.
class ProfileScope {
  public:
    explicit ProfileScope(
        std::string_view label,
        spdlog::level::level_enum level = spdlog::level::debug);
    ~ProfileScope();
    ProfileScope(const ProfileScope &) = delete;
    auto operator=(const ProfileScope &) -> ProfileScope & = delete;

  private:
    using Clock = std::chrono::steady_clock;
    std::string_view _label;
    spdlog::level::level_enum _level;
    std::optional<Clock::time_point> _start;
};

} // namespace balsa::visualization::vulkan
