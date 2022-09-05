#include "balsa/scene_graph/embedding_traits.hpp"
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <balsa/visualization/qt/vulkan/windows/scene.hpp>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>


class Scene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    void draw(const camera_type &, balsa::visualization::vulkan::Film &film) {

        static float value = 0.0;
        value += 0.0005f;
        if (value > 1.0f)
            value = 0.0f;
        auto col = colormap::transform::LavaWaves().getColor(value);
        set_clear_color(float(col.r), float(col.g), float(col.b));
        draw_background(film);
    }
};

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

    auto scene = std::make_shared<Scene>();
    balsa::visualization::qt::vulkan::windows::SceneWindow w(scene);
    w.setVulkanInstance(&inst);

    w.resize(1024, 768);
    w.show();

    using embedding_traits = balsa::scene_graph::embedding_traits3F;

    balsa::visualization::shaders::FlatShader<embedding_traits> fs;
    spdlog::info("Vertex shader");
    auto vdata = fs.vert_spirv();
    spdlog::info("Fragment shader");
    auto fdata = fs.frag_spirv();
    std::cout << vdata.size() << " " << fdata.size() << std::endl;
    fs.make_shader();
    //! [1]

    return app.exec();
}
