
#include "balsa/visualization/vulkan/scene_base.hpp"
//#include "balsa/visualization/vulkan/objects/node.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include <glm/mat4x4.hpp>
#include "balsa/scene_graph/camera.hpp"


namespace balsa::visualization::vulkan {
SceneBase::SceneBase() {}
// SceneBase::SceneBase(const std::shared_ptr<objects::Node> &root) : _root(root) {
// }
SceneBase::~SceneBase() {
}


void SceneBase::draw_background(Film &film) {

    VkClearValue clearValues[2];
    if (_do_clear_color) {
        clearValues[0].color = _clear_color;
    }
    if (_do_clear_depth) {
        clearValues[_do_clear_color ? 1 : 0].depthStencil = _clear_depth_stencil;
    }

    uint32_t clear_count = _do_clear_color + _do_clear_depth;


    const glm::uvec2 sz = film.swapChainImageSize();
    VkRenderPassBeginInfo rpBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = film.default_render_pass(),
        .framebuffer = film.current_framebuffer(),
        .renderArea = {
          // VkRect2D
          .offset = { // VkOffset2D
                      .x = int32_t(0),
                      .y = int32_t(0) },
          .extent = { // VkExtent2D
                      .width = uint32_t(sz.x),
                      .height = uint32_t(sz.y) },
        },
        .clearValueCount = clear_count,
        .pClearValues = clear_count > 0 ? clearValues : nullptr
    };
    if (bool(_root)) {
        //_root->draw(cam, film, glm::mat4x4);
    }

    vk::CommandBuffer cmdBuf = film.current_command_buffer();
    cmdBuf.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

    vkCmdEndRenderPass(cmdBuf);
}
void SceneBase::set_clear_color(float r, float g, float b, float a) {
    _clear_color.float32[0] = r;
    _clear_color.float32[1] = g;
    _clear_color.float32[2] = b;
    _clear_color.float32[3] = a;
}
void SceneBase::set_clear_color(int32_t r, int32_t g, int32_t b, int32_t a) {
    _clear_color.int32[0] = r;
    _clear_color.int32[1] = g;
    _clear_color.int32[2] = b;
    _clear_color.int32[3] = a;
}
void SceneBase::set_clear_color(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    _clear_color.uint32[0] = r;
    _clear_color.uint32[1] = g;
    _clear_color.uint32[2] = b;
    _clear_color.uint32[3] = a;
}
};// namespace balsa::visualization::vulkan
