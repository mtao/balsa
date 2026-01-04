#if !defined(BALSA_VISUALIZATION_QT_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_WINDOW_HPP

#include <QVulkanWindow>
#include <type_traits>


namespace balsa::visualization::qt::vulkan {


template<typename RendererType>
requires std::is_base_of_v<RendererType, QVulkanWindowRenderer>
class Window : public QVulkanWindow {
    virtual QVulkanWindowRenderer *createRenderer() override {
        return new RendererType(this);
    }
};

}// namespace balsa::visualization::qt::vulkan

#endif
