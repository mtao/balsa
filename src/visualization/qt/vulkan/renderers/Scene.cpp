#include "balsa/visualization/qt/vulkan/renderers/Scene.hpp"
#include "balsa/visualization/vulkan/Scene.hpp"
#include <QVulkanFunctions>
#include <QFile>
#include <spdlog/spdlog.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic pop
#include <balsa/visualization/qt/vulkan/Film.hpp>
#include <balsa/scene_graph/camera.hpp>

namespace balsa::visualization::qt::vulkan::renderers {


SceneRenderer::SceneRenderer(QVulkanWindow *w, std::shared_ptr<scene_type> scene, bool msaa)
  : m_window(w), _scene(std::move(scene)) {
    spdlog::info("Making renderer");
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
    spdlog::info("Making device functions");
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    spdlog::info("Done making device functions");
}

void SceneRenderer::initSwapChainResources() {
    spdlog::info("intiializing swapchain res");
}

void SceneRenderer::releaseSwapChainResources() {
    spdlog::info("releasing swapchain objects");
}

void SceneRenderer::releaseResources() {
    spdlog::info("releasing resources");
}

void SceneRenderer::startNextFrame() {

    //return;
    //spdlog::info("Do we have a scene? {}", bool(_scene));

    if (_scene) {

        Film film(*m_window);
        camera_type camera;

        //_scene->begin_render_pass(film);
        //_scene->draw(camera, film);
        //_scene->end_render_pass(film);
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
