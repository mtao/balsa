#if !defined(BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_SCENE_RENDERER_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_SCENE_RENDERER_HPP
#include <qt5/QtGui/qvulkanwindow.h>
#include "../Window.hpp"
#include "../../../vulkan/Scene.hpp"
#include "balsa/scene_graph/transformations/matrix_transformation.hpp"
namespace balsa::visualization::qt::vulkan::renderers {

class SceneRenderer : public QVulkanWindowRenderer {

  public:
    using embedding_traits = scene_graph::embedding_traits<3, float>;
    using transformation_type = scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = visualization::vulkan::Scene<transformation_type>;
    using camera_type = scene_graph::Camera<transformation_type>;
    SceneRenderer(QVulkanWindow *w, std::shared_ptr<scene_type> scene, bool msaa = false);
    ~SceneRenderer();

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;

    void startNextFrame() override;

  protected:
    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    std::shared_ptr<scene_type> _scene;
};

}// namespace balsa::visualization::qt::vulkan::renderers
#endif
