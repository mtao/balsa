#include <qt5/QtGui/qvulkanwindow.h>
#if !defined(BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_TRIANGLE_MESH_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_RENDERERS_TRIANGLE_MESH_HPP
#include "../window.hpp"
namespace balsa::visualization::qt::vulkan::renderers {

class TriangleMeshRenderer : public QVulkanWindowRenderer {

  public:
    TriangleMeshRenderer(QVulkanWindow *w, bool msaa = false);

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;

    void startNextFrame() override;

  protected:

    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

};

}// namespace balsa::visualization::qt::vulkan::renderers
#endif
