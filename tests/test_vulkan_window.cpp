#include "balsa/scene_graph/embedding_traits.hpp"
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <balsa/visualization/qt/vulkan/windows/scene.hpp>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <balsa/qt/spdlog_logger.hpp>

#include "example_vulkan_scene.hpp"
#include <balsa/visualization/qt/vulkan/film.hpp>
#include <QVulkanWindow>


//! [0]
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    spdlog::set_level(spdlog::level::trace);

    balsa::qt::activateSpdlogOutput();
    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    //! [0]
    QVulkanInstance inst;

    inst.setLayers(QByteArrayList()
                   << "VK_LAYER_GOOGLE_threading"
                   << "VK_LAYER_LUNARG_parameter_validation"
                   << "VK_LAYER_LUNARG_object_tracker"
                   << "VK_LAYER_LUNARG_core_validation"
                   << "VK_LAYER_LUNARG_image"
                   << "VK_LAYER_LUNARG_swapchain"
                   << "VK_LAYER_GOOGLE_unique_objects");

    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());
    //! [0]

    //! [1]

    balsa::visualization::qt::vulkan::windows::SceneWindow w;
    w.setVulkanInstance(&inst);

    auto scene = std::make_shared<HelloTriangleScene>();

    w.set_scene(scene);


    w.resize(1024, 768);
    w.show();

    // using embedding_traits = balsa::scene_graph::embedding_traits3F;

    // balsa::visualization::shaders::FlatShader<embedding_traits> fs;
    // spdlog::info("Vertex shader");
    // auto vdata = fs.vert_spirv();
    // spdlog::info("Fragment shader");
    // auto fdata = fs.frag_spirv();
    // std::cout << vdata.size() << " " << fdata.size() << std::endl;
    //  fs.make_shader();
    //! [1]

    return app.exec();
}
