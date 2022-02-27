#include "balsa/visualization/qt/vulkan/renderers/triangle_mesh.hpp"
#include "balsa/visualization/vulkan/scene.hpp"
#include <QVulkanFunctions>
#include <QFile>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <colormap/colormap.h>
#pragma GCC diagnostic pop
#include <balsa/visualization/qt/vulkan/film.hpp>
#include <balsa/visualization/vulkan/camera.hpp>

namespace balsa::visualization::qt::vulkan::renderers {


TriangleMeshRenderer::TriangleMeshRenderer(QVulkanWindow *w, bool msaa)
  : m_window(w) {
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
    _scene = std::make_shared<visualization::vulkan::Scene>();
}
TriangleMeshRenderer::~TriangleMeshRenderer() {}

void TriangleMeshRenderer::initResources() {
    m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
}

void TriangleMeshRenderer::initSwapChainResources() {
}

void TriangleMeshRenderer::releaseSwapChainResources() {
}

void TriangleMeshRenderer::releaseResources() {
}

void TriangleMeshRenderer::startNextFrame() {

    static float value = 0.0;
    value += 0.0005f;
    if (value > 1.0f)
        value = 0.0f;
    auto col = colormap::transform::LavaWaves().getColor(value);

    _scene->set_clear_color(float(col.r), float(col.g), float(col.b));

    Film film(*m_window);
    visualization::vulkan::Camera camera;

    _scene->draw(camera, film);

    //memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));


    // Do nothing else. We will just clear to green, changing the component on
    // every invocation. This also helps verifying the rate to which the thread
    // is throttled to. (The elapsed time between startNextFrame calls should
    // typically be around 16 ms. Note that rendering is 2 frames ahead of what
    // is displayed.)


    m_window->frameReady();
    m_window->requestUpdate();// render continuously, throttled by the presentation rate
}
}// namespace balsa::visualization::qt::vulkan::renderers
