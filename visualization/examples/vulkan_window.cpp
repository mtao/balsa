#include <QGuiApplication>
#include <QVulkanInstance>
#include <spdlog/spdlog.h>
#include <balsa/qt/spdlog_logger.hpp>
#include <balsa/visualization/qt/vulkan/window.hpp>
#include "vulkan_scene.hpp"

using namespace balsa::visualization;

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    spdlog::set_level(spdlog::level::trace);

    balsa::qt::activateSpdlogOutput();

    // The QVulkanInstance must outlive the QVulkanWindow.  Create it here
    // in main() so it is destroyed *after* the window.
    QVulkanInstance inst;
    inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
    if (!inst.create()) {
        spdlog::error("Failed to create QVulkanInstance");
        return 1;
    }

    qt::vulkan::Window window("Hello Triangle (Qt)");
    window.setVulkanInstance(&inst);

    auto scene = std::make_shared<HelloTriangleScene>();
    window.set_scene(scene);

    return window.exec();
}
