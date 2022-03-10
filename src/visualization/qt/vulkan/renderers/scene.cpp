#include "balsa/visualization/qt/vulkan/renderers/scene.hpp"
#include "balsa/visualization/vulkan/scene.hpp"
#include <QVulkanFunctions>
#include <QFile>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic pop
#include <balsa/visualization/qt/vulkan/film.hpp>
#include <balsa/scene_graph/camera.hpp>

namespace balsa::visualization::qt::vulkan::renderers {


SceneRenderer::SceneRenderer(QVulkanWindow *w, std::shared_ptr<scene_type> scene, bool msaa)
  : m_window(w), _scene(std::move(scene)) {
    if (msaa) {
        const QVector<int> counts = w->supportedSampleCounts();
        qDebug() << "Supported sample counts:" << counts;
        for (int s = 16; s >= 4; s /= 2) {
            if (counts.contains(s)) {
                qDebug("Requesting sample count %d", s);
                m_window->setSampleCount(s);
                break;
            }
        }
    }
}
SceneRenderer::~SceneRenderer() {}

void SceneRenderer::initResources() {
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
}

void SceneRenderer::initSwapChainResources() {
}

void SceneRenderer::releaseSwapChainResources() {
}

void SceneRenderer::releaseResources() {
}

void SceneRenderer::startNextFrame() {


    if (_scene) {

        Film film(*m_window);
        camera_type camera;

        _scene->draw_background(film);
        _scene->draw(camera, film);
    }

    // memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));


    // Do nothing else. We will just clear to green, changing the component on
    // every invocation. This also helps verifying the rate to which the thread
    // is throttled to. (The elapsed time between startNextFrame calls should
    // typically be around 16 ms. Note that rendering is 2 frames ahead of what
    // is displayed.)


    m_window->frameReady();
    m_window->requestUpdate();// render continuously, throttled by the presentation rate
}
}// namespace balsa::visualization::qt::vulkan::renderers
