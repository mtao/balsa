#include <QGuiApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <balsa/visualization/qt/vulkan/windows/triangle_mesh.hpp>


//! [0]
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);


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
    balsa::visualization::qt::vulkan::windows::TriangleMeshWindow w;
    w.setVulkanInstance(&inst);

    w.resize(1024, 768);
    w.show();
    //! [1]

    return app.exec();
}
