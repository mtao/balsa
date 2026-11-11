#include <catch2/catch_test_macros.hpp>

#include <balsa/visualization/vulkan/logging.hpp>
#include <spdlog/cfg/helpers.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <sstream>

namespace vulkan = balsa::visualization::vulkan;

namespace {
struct RestoreLogging {
    std::shared_ptr<spdlog::logger> original = spdlog::default_logger();
    spdlog::level::level_enum level = spdlog::get_level();
    ~RestoreLogging() {
        spdlog::drop("balsa.vulkan");
        spdlog::set_default_logger(original);
        spdlog::set_level(level);
    }
};
} // namespace

TEST_CASE("vulkan logger default routing", "[vulkan][logging]") {
    RestoreLogging restore;
    std::ostringstream output;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
    auto default_logger = std::make_shared<spdlog::logger>("default", sink);
    spdlog::set_default_logger(default_logger);
    spdlog::cfg::helpers::load_levels("info,balsa.vulkan=debug");
    default_logger->set_pattern("[%n] %v");

    const auto &logger = vulkan::logger();
    REQUIRE(logger == spdlog::get("balsa.vulkan"));
    REQUIRE(logger->sinks() == default_logger->sinks());
    CHECK(logger->level() == spdlog::level::debug);
    CHECK(default_logger->level() == spdlog::level::info);
    logger->debug("Vulkan detail");
    default_logger->debug("hidden");
    default_logger->info("normal output");
    CHECK(output.str()
          == "[balsa.vulkan] Vulkan detail\n[default] normal output\n");

    output.str("");
    { vulkan::ProfileScope profile("upload"); }
    CHECK(output.str().find("[balsa.vulkan] upload:") != std::string::npos);
    output.str("");
    { vulkan::ProfileScope profile("frame", spdlog::level::trace); }
    CHECK(output.str().empty());
    logger->set_level(spdlog::level::trace);
    { vulkan::ProfileScope profile("frame", spdlog::level::trace); }
    CHECK(output.str().find("[balsa.vulkan] frame:") != std::string::npos);
}

TEST_CASE("vulkan logger custom routing", "[vulkan][logging]") {
    RestoreLogging restore;
    std::ostringstream output;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
    auto custom = std::make_shared<spdlog::logger>("balsa.vulkan", sink);
    custom->set_level(spdlog::level::warn);
    custom->set_pattern("%v");
    spdlog::register_logger(custom);

    REQUIRE(vulkan::logger() == custom);
    CHECK(vulkan::logger()->level() == spdlog::level::warn);
    vulkan::logger()->debug("hidden");
    vulkan::logger()->warn("custom output");
    CHECK(output.str() == "custom output\n");
}
