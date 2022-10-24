#if defined(SKIP_ALL)
#else
#include <spdlog/spdlog.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <balsa/visualization/vulkan/pipeline_factory.hpp>
#include "example_vulkan_scene.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <colormap/colormap.h>
#pragma GCC diagnostic pop
#include <QtCore/QFile>

void balsa_visualization_tests_shaders_initialize_resources() {
    Q_INIT_RESOURCE(glsl);
}
namespace balsa::visualization::shaders {

template<scene_graph::concepts::embedding_traits ET>
class TriangleShader : public Shader<ET> {
  public:
    bool static_triangle_mode = true;
    TriangleShader(bool static_triangle_mode = true) : static_triangle_mode(static_triangle_mode) {

        balsa_visualization_tests_shaders_initialize_resources();
    }
    std::vector<uint32_t> vert_spirv() const override final;
    std::vector<uint32_t> frag_spirv() const override final;
};

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::vert_spirv() const {
    if (static_triangle_mode) {
        const static std::string fname = ":/tests/glsl/triangle.vert";
        return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderStage::Vertex);
    } else {
        const static std::string fname = ":/tests/glsl/input_vertex.vert";
        return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderStage::Vertex);
    }
}
template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::frag_spirv() const {
    const static std::string fname = ":/tests/glsl/triangle.frag";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderStage::Fragment);
}
}// namespace balsa::visualization::shaders

HelloTriangleScene::HelloTriangleScene() {}
// HelloTriangleScene::HelloTriangleScene(balsa::visualization::vulkan::Film &film)
HelloTriangleScene::~HelloTriangleScene() {
    if (device) {
        if (pipeline) {
            device.destroyPipeline(pipeline);
        }
        if (pipeline_layout) {
            device.destroyPipelineLayout(pipeline_layout);
        }
    }
}
void HelloTriangleScene::initialize(balsa::visualization::vulkan::Film &film) {
    if (!bool(device)) {
        device = film.device();
    }

    if (!bool(pipeline) || changed) {
        create_graphics_pipeline(film);
        changed = false;
    }

    _imgui.initialize(film);
}

void HelloTriangleScene::begin_render_pass(balsa::visualization::vulkan::Film &film) {
    initialize(film);
    static float value = 0.0;
    value += 0.0001f;
    if (value > 1.0f)
        value = 0.0f;
    auto col = colormap::transform::LavaWaves().getColor(value);
    set_clear_color(float(col.r), float(col.g), float(col.b));
    SceneBase::begin_render_pass(film);
    _imgui.begin_render_pass(film);
}

void HelloTriangleScene::draw(balsa::visualization::vulkan::Film &film) {

    assert(film.device() == device);


    auto cb = film.current_command_buffer();
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                    pipeline);

    vk::Viewport viewport{};
    auto extent = film.swapchain_image_size();

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.x;
    viewport.height = (float)extent.y;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cb.setViewport(0, { viewport });


    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = vk::Extent2D{ extent.x, extent.y };

    cb.setScissor(0, { scissor });


    cb.draw(3, 1, 0, 0);
    _imgui.draw(film);
}

void HelloTriangleScene::end_render_pass(balsa::visualization::vulkan::Film &film) {
    _imgui.end_render_pass(film);
}

void HelloTriangleScene::toggle_mode(balsa::visualization::vulkan::Film &film) {


    static_triangle_mode ^= true;
    changed = true;
    spdlog::warn("Toggline mode to static mode {}", static_triangle_mode);
}

void HelloTriangleScene::create_graphics_pipeline(balsa::visualization::vulkan::Film &film) {
    spdlog::info("HelloTriangleScene: starting to build piepline");
    // balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
    balsa::visualization::shaders::TriangleShader<balsa::scene_graph::embedding_traits2D> fs(static_triangle_mode);

    balsa::visualization::vulkan::PipelineFactory pf(film);
    std::tie(pipeline, pipeline_layout) = pf.generate(film, fs);
}

#endif
