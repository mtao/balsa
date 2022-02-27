#if !defined(BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_TRIANGLE_MESH_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_TRIANGLE_MESH_HPP
#include <qt5/QtGui/qvulkanwindow.h>
#include "../window.hpp"
#include "../../../vulkan/scene.hpp"
namespace balsa::visualization::qt::vulkan::renderers {

class TriangleMeshRenderer : public QVulkanWindowRenderer {

  public:
    TriangleMeshRenderer(QVulkanWindow *w, bool msaa = false);
    ~TriangleMeshRenderer();

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;

    void startNextFrame() override;

  protected:
    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    std::shared_ptr<visualization::vulkan::Scene> _scene;
};

}// namespace balsa::visualization::qt::vulkan::renderers
#endif
