#include "balsa/visualization/vulkan/logging.hpp"

#include <mutex>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

auto logger() -> std::shared_ptr<spdlog::logger> {
    if (auto existing = spdlog::get("balsa.vulkan")) return existing;
    static std::mutex creation_mutex;
    const std::scoped_lock lock(creation_mutex);
    if (auto existing = spdlog::get("balsa.vulkan")) return existing;
    auto created = spdlog::default_logger()->clone("balsa.vulkan");
    spdlog::apply_logger_env_levels(created);
    spdlog::register_logger(created);
    return created;
}

ProfileScope::ProfileScope(std::string_view label,
                           spdlog::level::level_enum level)
  : _label(label), _level(level) {
    if (logger()->should_log(level)) _start = Clock::now();
}

ProfileScope::~ProfileScope() {
    if (_start) {
        logger()->log(
            _level,
            "{}: {:.3f} ms",
            _label,
            std::chrono::duration<double, std::milli>(Clock::now() - *_start)
                .count());
    }
}

} // namespace balsa::visualization::vulkan
