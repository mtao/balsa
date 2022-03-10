#include "balsa/visualization/qt/vulkan/windows/scene.hpp"
#include "balsa/visualization/qt/vulkan/renderers/scene.hpp"


namespace balsa::visualization::qt::vulkan::windows {
QVulkanWindowRenderer *SceneWindow::createRenderer() {
    return new renderers::SceneRenderer(this, _scene);
}
}// namespace balsa::visualization::qt::vulkan::windows
