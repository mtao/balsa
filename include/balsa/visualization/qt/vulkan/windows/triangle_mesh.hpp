#if !defined(BALSA_VISUALIZATION_QT_VULKAN_WINDOWS_TRIANGLE_MESH_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_WINDOWS_TRIANGLE_MESH_HPP
#include <QVulkanWindow>
#include "../renderers/triangle_mesh.hpp"
namespace balsa::visualization::qt::vulkan::windows {


class TriangleMeshRenderer;
class TriangleMeshWindow : public QVulkanWindow {
    QVulkanWindowRenderer *createRenderer() override;
};
}// namespace balsa::visualization::qt::vulkan::windows
#endif
