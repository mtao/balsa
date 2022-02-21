#include "balsa/visualization/qt/vulkan/windows/triangle_mesh.hpp"
#include "balsa/visualization/qt/vulkan/renderers/triangle_mesh.hpp"


namespace balsa::visualization::qt::vulkan::windows {
QVulkanWindowRenderer *TriangleMeshWindow::createRenderer() {
    return new renderers::TriangleMeshRenderer(this);
}
}// namespace balsa::visualization::qt::vulkan::windows
