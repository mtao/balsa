#if defined(SKIP_ALL)
#else
#include <spdlog/spdlog.h>
#include <balsa/visualization/shaders/Flat.hpp>
#include <balsa/visualization/vulkan/utils/PipelineFactory.hpp>
#include "balsa/visualization/vulkan/utils/BufferCopier.hpp"
#include <balsa/visualization/vulkan/utils/find_memory_type.hpp>
#include <range/v3/range/conversion.hpp>
#include <balsa/logging/stopwatch.hpp>
#include "example_vulkan_scene.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <colormap/colormap.h>
#pragma GCC diagnostic pop
#include <QtCore/QFile>
#include <spdlog/stopwatch.h>

#include <glm/glm.hpp>
#include <array>

namespace {
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    static vk::VertexInputBindingDescription get_binding_description();
    static std::vector<vk::VertexInputAttributeDescription> get_attribute_descriptions();
};
vk::VertexInputBindingDescription Vertex::get_binding_description() {
    vk::VertexInputBindingDescription binding_description{
        /*.binding = */ 0,
        /*.stride = */ sizeof(Vertex),
        /*.inputRate = */ vk::VertexInputRate::eVertex
    };
    return binding_description;
}
std::vector<vk::VertexInputAttributeDescription> Vertex::get_attribute_descriptions() {

    std::vector<vk::VertexInputAttributeDescription> ret(2);
    auto &pos = ret[0];
    auto &col = ret[1];
    pos.binding = 0;
    pos.location = 0;
    pos.format = vk::Format::eR32G32B32A32Sfloat;
    col.binding = 0;
    col.location = 1;
    col.format = vk::Format::eR32G32B32Sfloat;
    col.offset = offsetof(Vertex, color);
    return ret;
}

// const std::vector<Vertex> vertices = {
//     { { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
//     { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
//     { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } }
// };
const std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } }
};
const std::vector<uint16_t> indices = {
    0,
    1,
    2,
    2,
    3,
    0
};
}// namespace
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
    if (static_triangle_mode) {
        const static std::string fname = ":/tests/glsl/triangle.frag";
        return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderStage::Fragment);
    } else {
        const static std::string fname = ":/tests/glsl/input_vertex.frag";
        return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderStage::Fragment);
    }
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

    //_imgui.initialize(film);
}

void HelloTriangleScene::begin_render_pass(balsa::visualization::vulkan::Film &film) {
    initialize(film);
    static float value = 0.0;
    // value += 0.0001f;
    // if (value > 1.0f)
    //     value = 0.0f;
    auto col = colormap::transform::LavaWaves().getColor(value);
    set_clear_color(float(col.r), float(col.g), float(col.b));
    SceneBase::begin_render_pass(film);
    //_imgui.begin_render_pass(film);
}

void HelloTriangleScene::draw(balsa::visualization::vulkan::Film &film) {

    assert(film.device() == device);


    auto cb = film.current_command_buffer();
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                    pipeline);

    if (!static_triangle_mode) {
        vertex_buffer_views.bind(cb);
    }

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

    // cb.setScissor(0, { scissor });


    if (static_triangle_mode) {
        cb.draw(vertices.size(), 1, 0, 0);
    } else {
        if (index_buffer_view.has_buffer()) {
            auto [buf, off, type] = index_buffer_view.bind_info();
            if (buf) {
                cb.bindIndexBuffer(buf, off, type);
                cb.drawIndexed(indices.size(), 1, 0, 0, 0);
            }
        } else {
            cb.draw(vertices.size(), 1, 0, 0);
        }
    }
    //_imgui.draw(film);
}

void HelloTriangleScene::end_render_pass(balsa::visualization::vulkan::Film &film) {

    scene_type::end_render_pass(film);

    //_imgui.end_render_pass(film);
}

void HelloTriangleScene::toggle_mode(balsa::visualization::vulkan::Film &) {


    static_triangle_mode ^= true;
    changed = true;
    spdlog::warn("Toggline mode to static mode {}", static_triangle_mode);
}

void HelloTriangleScene::create_graphics_pipeline(balsa::visualization::vulkan::Film &film) {

    auto sw = balsa::logging::hierarchical_stopwatch("HelloTriangleScene::create_graphics_pipeline");
    spdlog::info("HelloTriangleScene: starting to build pipeline");
    // balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
    balsa::visualization::shaders::TriangleShader<balsa::scene_graph::embedding_traits2D> fs(static_triangle_mode);

    spdlog::info("pipeline");
    balsa::visualization::vulkan::utils::PipelineFactory pf(film);
    spdlog::info("static triangle");

    vertex_buffer_views.clear();

    balsa::visualization::vulkan::VertexBufferView &vertex_buffer_view = vertex_buffer_views.emplace();
    {
        vertex_buffer_view.set_binding_description(Vertex::get_binding_description());
        auto ad = Vertex::get_attribute_descriptions();
        vertex_buffer_view.set_attribute_descriptions(std::span(ad.begin(), ad.end()));
    }


    vertex_buffer_views.build_descriptions();

    if (!static_triangle_mode) {
        vertex_buffer_views.set_vertex_inputs(pf);
    }
    auto bc = balsa::visualization::vulkan::utils::BufferCopier::from_film(film);
    if (!vertex_buffer_view.has_buffer()) {

        std::shared_ptr<balsa::visualization::vulkan::Buffer> vertex_buffer;
        vertex_buffer = std::make_shared<balsa::visualization::vulkan::Buffer>(film);

        vk::DeviceSize data_size = sizeof(decltype(vertices)::value_type) * vertices.size();
        if (!use_staging_buffer) {
            auto sw = balsa::logging::hierarchical_stopwatch("HelloTriangleScene::create_graphics_pipeline::vertex_copy_buffer");
            bc.copy_direct(*vertex_buffer, (void *)vertices.data(), data_size, vk::BufferUsageFlagBits::eVertexBuffer);
            vertex_buffer_view.set_buffer(vertex_buffer);


        } else {
            auto sw = balsa::logging::hierarchical_stopwatch("HelloTriangleScene::create_graphics_pipeline::vertex_staging_buffer");
            bc.copy_with_staging_buffer(*vertex_buffer, (void *)vertices.data(), data_size, vk::BufferUsageFlagBits::eVertexBuffer);

            vertex_buffer_view.set_buffer(vertex_buffer);
        }
    }
    if (!index_buffer_view.has_buffer()) {

        auto sw = balsa::logging::hierarchical_stopwatch("HelloTriangleScene::create_graphics_pipeline::index_staging_buffer");
        std::shared_ptr<balsa::visualization::vulkan::Buffer> index_buffer;
        index_buffer = std::make_shared<balsa::visualization::vulkan::Buffer>(film);

        vk::DeviceSize data_size = sizeof(decltype(indices)::value_type) * indices.size();

        spdlog::info("Copying ndex buffer");
        bc.copy_with_staging_buffer(*index_buffer, (void *)indices.data(), data_size, vk::BufferUsageFlagBits::eIndexBuffer);

        spdlog::info("setting");
        index_buffer_view.set_buffer(index_buffer);
    }


    if (pipeline) {
        device.destroyPipeline(pipeline);
    }
    if (pipeline_layout) {
        device.destroyPipelineLayout(pipeline_layout);
    }
    std::tie(pipeline, pipeline_layout) = pf.generate(film, fs);
}

#endif
