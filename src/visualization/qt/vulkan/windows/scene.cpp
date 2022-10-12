#include "balsa/visualization/qt/vulkan/windows/scene.hpp"
#include "balsa/visualization/qt/vulkan/renderers/scene.hpp"


namespace balsa::visualization::qt::vulkan::windows {

SceneWindow::SceneWindow(std::shared_ptr<scene_type> scene) : _scene(std::move(scene)) {
}


QVulkanWindowRenderer *SceneWindow::createRenderer() {
    return new renderers::SceneRenderer(this, _scene);
}


void SceneWindow::set_scene(std::shared_ptr<scene_type> scene) {
    _scene = std::move(scene);
}

}// namespace balsa::visualization::qt::vulkan::windows
