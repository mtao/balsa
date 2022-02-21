#include "balsa/visualization/qt/vulkan/renderers/triangle_mesh.hpp"
#include <QVulkanFunctions>
#include <QFile>
#include <colormap/colormap.h>

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
}

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
    value += 0.005f;
    if (value > 1.0f)
        value = 0.0f;
    auto col = colormap::transform::LavaWaves().getColor(value);

    VkClearColorValue clearColor = { { float(col.r),float(col.g),float(col.b), 1.0f } };
    VkClearDepthStencilValue clearDS = { 1.0f, 0 };
    VkClearValue clearValues[2];
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo;

    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.pNext = nullptr;
    rpBeginInfo.renderPass = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer = m_window->currentFramebuffer();
    const QSize sz = m_window->swapChainImageSize();
    rpBeginInfo.renderArea.extent.width = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = 2;
    rpBeginInfo.pClearValues = clearValues;
    VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
    m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Do nothing else. We will just clear to green, changing the component on
    // every invocation. This also helps verifying the rate to which the thread
    // is throttled to. (The elapsed time between startNextFrame calls should
    // typically be around 16 ms. Note that rendering is 2 frames ahead of what
    // is displayed.)

    m_devFuncs->vkCmdEndRenderPass(cmdBuf);

    m_window->frameReady();
    m_window->requestUpdate();// render continuously, throttled by the presentation rate
}
}// namespace balsa::visualization::qt::vulkan::renderers
