#include "balsa/visualization/vulkan/utils/PipelineFactory.hpp"
#include "balsa/visualization/vulkan/utils/VertexBufferViewCollection.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/visualization/shaders/AbstractShader.hpp"
#include <spdlog/spdlog.h>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/action/push_back.hpp>
#include "balsa/logging/stopwatch.hpp"


namespace balsa::visualization::vulkan::utils {
namespace {
    vk::DescriptorType balsa_to_vk(shaders::DescriptorBinding::Type stage) {
        switch (stage) {
        case shaders::DescriptorBinding::Type::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        default:
            return vk::DescriptorType::eUniformBuffer;
        }
    }
    vk::ShaderStageFlagBits balsa_to_vk(shaders::ShaderStage stage) {
        switch (stage) {
        case shaders::ShaderStage::Vertex:
            return vk::ShaderStageFlagBits::eVertex;
        case shaders::ShaderStage::Fragment:
            return vk::ShaderStageFlagBits::eFragment;
        case shaders::ShaderStage::Geometry:
            return vk::ShaderStageFlagBits::eGeometry;
        case shaders::ShaderStage::Compute:
            return vk::ShaderStageFlagBits::eCompute;
        case shaders::ShaderStage::TessellationControl:
            return vk::ShaderStageFlagBits::eTessellationControl;
        case shaders::ShaderStage::TessellationEvaluation:
            return vk::ShaderStageFlagBits::eTessellationEvaluation;
        default:
            return vk::ShaderStageFlagBits::eVertex;
        }
    }
}// namespace


struct PipelineFactory::ShaderCollection {
    struct Shader {
        std::string name;
        vk::ShaderStageFlagBits stage;
        vk::ShaderModule mod;
    };

  public:
    ShaderCollection(const vk::Device &device, const shaders::AbstractShader &shader);
    ~ShaderCollection();
    const std::vector<Shader> &shaders() const { return m_shaders; }

  private:
    const vk::Device &m_device;
    std::vector<Shader> m_shaders;
};

PipelineFactory::ShaderCollection::ShaderCollection(const vk::Device &device, const shaders::AbstractShader &shader) : m_device(device) {

    // auto sw = balsa::logging::hierarchical_stopwatch("PipelineFactory::generate");

    auto spirv = shader.compile_spirv();

    auto shader_to_module =
      [&device](const shaders::SpirvShader &shader) -> vk::ShaderModule {
        const auto &spv = shader.data;
        vk::ShaderModuleCreateInfo ci;
        ci.setCodeSize(sizeof(uint32_t) * spv.size());
        ci.setPCode(spv.data());
        return device.createShaderModule(ci);
    };

    std::vector<vk::ShaderModule> shader_modules;
    {

        auto sw = balsa::logging::hierarchical_stopwatch("Building shader modules");
        shader_modules = spirv | ranges::views::transform(shader_to_module)
                         | ranges::to_vector;
    }


    m_shaders =
      ranges::views::zip(spirv, shader_modules) | ranges::views::transform([&](const auto &pr) -> Shader {
          const auto &[spirv, shader_module] = pr;
          return Shader{
              .name = spirv.name, .stage = balsa_to_vk(spirv.stage), .mod = shader_module
          };
      })
      | ranges::to<std::vector>();
}
PipelineFactory::ShaderCollection::~ShaderCollection() {
    for (auto &shader : m_shaders) {
        m_device.destroyShaderModule(shader.mod);
    }
}

struct PipelineFactory::DescriptorSetLayoutCollection {

  public:
    DescriptorSetLayoutCollection(const vk::Device &device);
    void add_layout(const vk::DescriptorSetLayoutCreateInfo &layout_create_infos);
    ~DescriptorSetLayoutCollection();
    const std::vector<vk::DescriptorSetLayout> &layouts() const { return m_data; }
    void add(const shaders::AbstractShader &shader);

  private:
    const vk::Device &m_device;
    std::vector<vk::DescriptorSetLayout> m_data;
};

PipelineFactory::DescriptorSetLayoutCollection::DescriptorSetLayoutCollection(const vk::Device &device) : m_device(device) {
}

PipelineFactory::DescriptorSetLayoutCollection::~DescriptorSetLayoutCollection() {
    for (auto &d : m_data) {
        m_device.destroyDescriptorSetLayout(d);
    }
}
void PipelineFactory::DescriptorSetLayoutCollection::add(
  const shaders::AbstractShader &shader) {
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;

    auto bindings = shader.get_descriptor_bindings();
    layout_bindings = bindings | ranges::v3::view::transform([&](const auto &binding) -> vk::DescriptorSetLayoutBinding {
                          vk::DescriptorSetLayoutBinding ret;

                          ret.setBinding(binding.binding);
                          ret.setDescriptorType(balsa_to_vk(binding.type));
                          ret.setDescriptorCount(binding.count);
                          vk::ShaderStageFlags stages{};
                          if (binding.stages.empty()) {
                              stages = vk::ShaderStageFlagBits::eAll;
                          } else {
                              for (const auto &bstage : binding.stages) {
                                  stages |= balsa_to_vk(bstage);
                              }
                          }
                          ret.setStageFlags(stages);
                          ret.setImmutableSamplers(nullptr);
                          return ret;
                      })
                      | ranges::to<std::vector>;
    // TODO: read these from shader
    vk::DescriptorSetLayoutCreateInfo create_info;
    create_info.setBindings(layout_bindings);
    m_data.emplace_back(m_device.createDescriptorSetLayout(create_info));
}


PipelineFactory::PipelineFactory(Film &film) : m_device(film.device()) {
    auto sw = balsa::logging::hierarchical_stopwatch("PipelineFactory::PipelineFactory");
    {
        auto &ci = vertex_input;
        ci.setVertexBindingDescriptionCount(0);
        ci.setVertexBindingDescriptions(nullptr);
        ci.setVertexAttributeDescriptionCount(0);
        ci.setVertexAttributeDescriptions(nullptr);
    }

    {
        auto &ci = input_assembly;
        ci.setTopology(vk::PrimitiveTopology::eTriangleList);
        // allow for triangle strip type tools with special reset indices
        ci.setPrimitiveRestartEnable(VK_FALSE);
    }
    auto ssize = film.swapchain_image_size();
    swapchain_extent = vk::Extent2D(ssize.x, ssize.y);
    {
        scissor.setOffset({ 0, 0 });
        scissor.setExtent(swapchain_extent);
    }

    {
        viewport.setX(0.0f);
        viewport.setY(0.0f);
        viewport.setWidth(swapchain_extent.width);
        viewport.setHeight(swapchain_extent.height);
        viewport.setMinDepth(0.0f);
        viewport.setMaxDepth(1.0f);
    }
    {
        auto &ci = viewport_state;
        ci.setViewportCount(1);
        ci.setPViewports(&viewport);
        ci.setScissorCount(1);
        ci.setPScissors(&scissor);
    }

    {
        auto &ci = rasterization_state;
        ci.setDepthClampEnable(VK_FALSE);
        ci.setRasterizerDiscardEnable(VK_FALSE);
        ci.setLineWidth(1.0f);
        ci.setPolygonMode(vk::PolygonMode::eFill);
        ci.setCullMode(vk::CullModeFlagBits::eBack);
        ci.setFrontFace(vk::FrontFace::eClockwise);

        ci.setDepthBiasEnable(VK_FALSE);
        ci.setDepthBiasConstantFactor(0.0f);
        ci.setDepthBiasClamp(0.0f);
        ci.setDepthBiasSlopeFactor(0.0f);
    }

    {
        auto &ci = multisampling_state;
        ci.setSampleShadingEnable(VK_FALSE);
        ci.setRasterizationSamples(vk::SampleCountFlagBits::e1);
        ci.setMinSampleShading(1.0f);
        ci.setPSampleMask(nullptr);
        ci.setAlphaToCoverageEnable(VK_FALSE);
        ci.setAlphaToOneEnable(VK_FALSE);
    }

    {
        auto &s = color_blend_attachment;
        s.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        s.setBlendEnable(VK_FALSE);
        s.setSrcColorBlendFactor(vk::BlendFactor::eOne);
        s.setDstColorBlendFactor(vk::BlendFactor::eZero);

        s.setColorBlendOp(vk::BlendOp::eAdd);
        s.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        s.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        s.setAlphaBlendOp(vk::BlendOp::eAdd);
    }

    {
        auto &ci = color_blend_state;
        ci.setLogicOpEnable(VK_FALSE);
        ci.setLogicOp(vk::LogicOp::eCopy);
        ci.setAttachmentCount(1);
        ci.setPAttachments(&color_blend_attachment);
        ci.setBlendConstants({ 0.f, 0.f, 0.f, 0.f });
    }

    dynamic_states = {
        vk::DynamicState::eViewport, vk::DynamicState::eLineWidth
    };

    {
        dynamic_state.setDynamicStateCount(dynamic_states.size());
        dynamic_state.setPDynamicStates(dynamic_states.data());
    }

    {
        auto &ci = pipeline_layout;
        ci.setSetLayoutCount(
          0);// automatic bindings have cool names, but why
        ci.setPushConstantRangeCount(0);
    }

    {
        auto &ci = graphics_pipeline;

        ci.setStageCount(2);
        ci.setPVertexInputState(&vertex_input);
        ci.setPInputAssemblyState(&input_assembly);
        ci.setPViewportState(&viewport_state);
        ci.setPRasterizationState(&rasterization_state);
        ci.setPMultisampleState(&multisampling_state);
        ci.setPColorBlendState(&color_blend_state);
        ci.setPDynamicState(&dynamic_state);
        ci.setRenderPass(film.default_render_pass());
        ci.setSubpass(0);
        ci.setBasePipelineHandle(VK_NULL_HANDLE);
    }
}
std::tuple<vk::Pipeline, vk::PipelineLayout>
  PipelineFactory::generate() {
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;

    std::tuple<vk::Pipeline, vk::PipelineLayout> ret;
    auto &[pipeline, pipeline_layout] = ret;

    { // hook the accumulated data (like shaders and descriptions) into the PipelineCreateInfo

      {
        vertex_input.setVertexBindingDescriptions(m_binding_descriptions);
    vertex_input.setVertexAttributeDescriptions(m_attribute_descriptions);
}


{
    auto shaders_to_stages = [&](const ShaderCollection::Shader &shader) -> vk::PipelineShaderStageCreateInfo {
        vk::PipelineShaderStageCreateInfo stage;
        stage.setStage(shader.stage);
        stage.setModule(shader.mod);
        stage.setPName(shader.name.c_str());
        return stage;
    };
    shader_stages = m_shaders->shaders() | ranges::views::transform(shaders_to_stages) | ranges::to<std::vector>;
    graphics_pipeline.setStages(shader_stages);
}
{
    auto &ci = this->pipeline_layout;
    pipeline_layout = m_device.createPipelineLayout(ci);
    graphics_pipeline.setLayout(pipeline_layout);
}

{

    this->pipeline_layout.setSetLayouts(m_descriptor_set_layouts->layouts());
}
}// namespace balsa::visualization::vulkan::utils

// finally do the generation


{
    auto res = m_device.createGraphicsPipeline(VK_NULL_HANDLE, graphics_pipeline);

    if (res.result == vk::Result::eSuccess) {
        pipeline = res.value;
    } else {
        spdlog::error("was unable to create graphics pipeline");
    }
}
return ret;
}

PipelineFactory::~PipelineFactory() = default;


void PipelineFactory::build_shaders(const shaders::AbstractShader &shader) {
    m_shaders = std::make_unique<ShaderCollection>(m_device, shader);
}

void PipelineFactory::set_vertex_inputs(const VertexBufferViewCollection &collection) {
    m_binding_descriptions = collection.binding_descriptions();
    m_attribute_descriptions = collection.attribute_descriptions();
}

void PipelineFactory::clear_vertex_inputs() {
    m_binding_descriptions.clear();
    m_attribute_descriptions.clear();
}
void PipelineFactory::set_vertex_input(const VertexBufferView &view) {
    m_binding_descriptions.clear();
    m_attribute_descriptions.clear();
    add_vertex_input(view);
}

void PipelineFactory::add_vertex_input(const VertexBufferView &view) {
    uint32_t binding = m_binding_descriptions.size();

    auto set_binding = [&](vk::VertexInputAttributeDescription attr) -> vk::VertexInputAttributeDescription {
        attr.setBinding(binding);
        return attr;
    };
    m_binding_descriptions.emplace_back(view.binding_description());
    m_binding_descriptions.back().setBinding(binding);
    m_attribute_descriptions = std::move(m_attribute_descriptions) | ranges::push_back(view.attribute_descriptions() | ranges::views::transform(set_binding));
}

void PipelineFactory::add_descriptor_set_layout(const shaders::AbstractShader &shader) {
    if (!m_descriptor_set_layouts) {
        m_descriptor_set_layouts = std::make_unique<DescriptorSetLayoutCollection>(m_device);
    }
    m_descriptor_set_layouts->add(shader);
}
}// namespace balsa::visualization::vulkan::utils
