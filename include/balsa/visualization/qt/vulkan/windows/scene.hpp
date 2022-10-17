#if !defined(BALSA_VISUALIZATION_QT_VULKAN_WINDOWS_SCENE_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_WINDOWS_SCENE_HPP
#include <QVulkanWindow>
#include "../renderers/scene.hpp"
namespace balsa::visualization::qt::vulkan::windows {


class SceneRenderer;
class SceneWindow : public QVulkanWindow {
  public:
    using scene_type = renderers::SceneRenderer::scene_type;
    QVulkanWindowRenderer *createRenderer() override;

    SceneWindow(std::shared_ptr<scene_type> scene = nullptr);

    void set_scene(std::shared_ptr<scene_type> scene);


  private:
    std::shared_ptr<scene_type> _scene;
};
}// namespace balsa::visualization::qt::vulkan::windows
#endif
