
#include "balsa/visualization/vulkan/scene_base.hpp"
// #include "balsa/visualization/vulkan/objects/node.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include <glm/mat4x4.hpp>
#include "balsa/scene_graph/camera.hpp"


namespace balsa::visualization::vulkan {
SceneBase::SceneBase() {}
// SceneBase::SceneBase(const std::shared_ptr<objects::Node> &root) : _root(root) {
// }
SceneBase::~SceneBase() {
}

void SceneBase::release_vulkan_resources() {
    // Default: nothing to release.  Subclasses override as needed.
}

void SceneBase::end_render_pass(Film &film) {
    vk::CommandBuffer cmdBuf = film.current_command_buffer();
    cmdBuf.endRenderPass();
}

void SceneBase::initialize(Film &) {
}

void SceneBase::begin_render_pass(Film &film) {

    // Build clear values indexed by attachment index.  The render pass
    // layout from NativeFilm::create_render_pass is:
    //   [0] color   — MSAA image or swapchain image  (loadOp = eClear)
    //   [1] depth   — depth/stencil image, only if enabled  (loadOp = eClear)
    //   [2] resolve — swapchain image, only if MSAA  (loadOp = eDontCare)
    //
    // Per the Vulkan spec, clearValueCount must be > the largest attachment
    // index with loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR.  Clear values are
    // indexed by attachment index, NOT packed contiguously — so depth is
    // always at index 1 regardless of whether color is being cleared.

    const bool has_depth = film.has_depth_stencil();
    const bool clear_depth = _do_clear_depth && has_depth;

    // clearValueCount must cover all attachments that have eClear loadOp.
    // Attachment 0 (color) always has eClear; attachment 1 (depth) has
    // eClear when present.  We size the array to the highest clear index + 1.
    constexpr uint32_t COLOR_INDEX = 0;
    constexpr uint32_t DEPTH_INDEX = 1;

    VkClearValue clearValues[2] = {};
    uint32_t clear_count = 0;

    if (_do_clear_color) {
        clearValues[COLOR_INDEX].color = _clear_color;
        clear_count = COLOR_INDEX + 1;
    }
    if (clear_depth) {
        clearValues[DEPTH_INDEX].depthStencil = _clear_depth_stencil;
        clear_count = DEPTH_INDEX + 1;
    }


    const glm::uvec2 sz = film.swapchain_image_size();
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

    vk::CommandBuffer cmdBuf = film.current_command_buffer();
    cmdBuf.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);
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
