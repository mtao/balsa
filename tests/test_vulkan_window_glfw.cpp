#include <balsa/scene_graph/embedding_traits.hpp>
#include <span>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace balsa::visualization::shaders {

template<scene_graph::concepts::embedding_traits ET>
class TriangleShader : public Shader<ET> {
  public:
    TriangleShader() {}
    std::vector<uint32_t> vert_spirv() const override final;
    std::vector<uint32_t> frag_spirv() const override final;
};

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::vert_spirv() const {
    const static std::string fname = ":/glsl/triangle.vert";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Vertex);
}
template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::frag_spirv() const {
    const static std::string fname = ":/glsl/triangle.frag";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Fragment);
}
}// namespace balsa::visualization::shaders
#include <balsa/visualization/vulkan/scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <GLFW/glfw3.h>

class Scene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    void draw(const camera_type &, balsa::visualization::vulkan::Film &film) {

        static float value = 0.0;
        value += 0.0005f;
        if (value > 1.0f)
            value = 0.0f;
        auto col = colormap::transform::LavaWaves().getColor(value);
        set_clear_color(float(col.r), float(col.g), float(col.b));
        draw_background(film);
    }
};

using namespace balsa::visualization;


// this shim proves why protected is stupid
// but maybe it's actually really cool - testing shims only add a little bit of
// work for writing tests, but not so much that writing tests feels burdensome
class GLFWFilmTestShim : public glfw::vulkan::Film {
  public:
    using Film::Film;
    using Film::swapchain;
};


class HelloTriangleApplication {
  public:

    HelloTriangleApplication() : window(make_window()) {
        film = std::make_unique<GLFWFilmTestShim>(window);
        create_graphics_pipeline();
        //debugMessenger = *film->_debug_messenger_raii;
        //physicalDevice = *film->physical_device();
        //device = *film->device();


    }

    void run() { mainLoop(); }
    ~HelloTriangleApplication() {
        glfwDestroyWindow(window);
    }

  private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow *window = nullptr;
    std::unique_ptr<GLFWFilmTestShim> film;
    // std::unique_ptr<glfw::vulkan::Film> film;
    Scene scene;


    vk::raii::PipelineLayout pipeline_layout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;


    GLFWwindow *make_window() {
        // don't want to make glfw accidentally create an opengl context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow *window =
          glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        const char *description;
        int code = glfwGetError(&description);
        if (code != GLFW_NO_ERROR) {
            spdlog::error("glfw init error: {}", description);
            throw std::runtime_error("Could not start GLFW");
        }
        return window;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            spdlog::info("Doing a frame");
            glfwPollEvents();
            //old_draw_frame();
            // film->pre_draw();
            // draw_frame();
            // film->post_draw();
            //  film->device().waitIdle();
        }
    }


    void record_command_buffer() {
        auto cb = film->current_command_buffer();

        vk::ClearValue clear_color =
          vk::ClearColorValue{ std::array<float, 4>{ 0.f, 0.f, 0.f, 1.f } };
        vk::RenderPassBeginInfo rpi;
        {
            rpi.setRenderPass(film->default_render_pass());
            rpi.setFramebuffer(film->current_framebuffer());
            rpi.renderArea.setOffset({ 0, 0 });
            auto extent = film->swapchain_image_size();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        *pipeline);

        cb.draw(3, 1, 0, 0);
    }
    void old_record_command_buffer() {
        auto cb = film->current_command_buffer();
        vk::CommandBufferBeginInfo bi;
        {
            bi.setFlags({});// not used
            bi.setPInheritanceInfo(nullptr);
            cb.begin(bi);
        }

        vk::ClearValue clear_color =
          vk::ClearColorValue{ std::array<float, 4>{ 0.f, 0.f, 0.f, 1.f } };
        vk::RenderPassBeginInfo rpi;
        {
            rpi.setRenderPass(film->default_render_pass());
            rpi.setFramebuffer(film->current_framebuffer());
            rpi.renderArea.setOffset({ 0, 0 });
            auto extent = film->swapchain_image_size();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        *pipeline);

        cb.draw(3, 1, 0, 0);

        cb.endRenderPass();

        cb.end();
    }

    void old_draw_frame() {
        auto device = film->device();
        const auto &frame = film->frame_resources();

        vk::Result result;
        result = device.waitForFences(*frame.fence_raii, VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Fence failed");
        }
        device.resetFences(*frame.fence_raii);

        uint32_t image_index;
        result = device.acquireNextImageKHR(film->swapchain(), UINT64_MAX, *frame.image_semaphore_raii, VK_NULL_HANDLE, &image_index);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Failed to acquire image");
        }
        auto command_buffer = film->current_command_buffer();

        record_command_buffer();

        vk::SubmitInfo si;
        vk::PipelineStageFlags wait_stages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };
        {
            si.setWaitSemaphores(*frame.image_semaphore_raii);

            si.setWaitDstStageMask(*wait_stages);
            si.setCommandBuffers(command_buffer);
            si.setSignalSemaphores(*frame.draw_semaphore_raii);
        }

        film->graphics_queue().submit(si, *frame.fence_raii);

        vk::PresentInfoKHR present;
        present.setWaitSemaphores(*frame.draw_semaphore_raii);

        present.setSwapchains(film->swapchain());
        present.setImageIndices(image_index);

        present.setPResults(nullptr);
        result = film->present_queue().presentKHR(present);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Failed to present");
        }
    }

    void draw_frame() {
        record_command_buffer();
    }
    void create_graphics_pipeline() {
        balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
        auto vert_spv = fs.vert_spirv();
        auto frag_spv = fs.frag_spirv();

        auto create_shader_module =
          [&](const std::vector<uint32_t> &spv) -> vk::raii::ShaderModule {
            vk::ShaderModuleCreateInfo ci;
            ci.setCodeSize(sizeof(uint32_t) * spv.size());
            ci.setPCode(spv.data());
            return film->device_raii().createShaderModule(ci);
        };

        vk::raii::ShaderModule vert_shader_module = create_shader_module(vert_spv);
        vk::raii::ShaderModule frag_shader_module = create_shader_module(frag_spv);
        vk::PipelineShaderStageCreateInfo shader_stages[2];

        const std::string name = "main";

        {
            auto &vert_stage = shader_stages[0];
            vert_stage.setStage(vk::ShaderStageFlagBits::eVertex);
            vert_stage.setModule(*vert_shader_module);
            vert_stage.setPName(name.c_str());

            auto &frag_stage = shader_stages[1];
            frag_stage.setStage(vk::ShaderStageFlagBits::eFragment);
            frag_stage.setModule(*frag_shader_module);
            frag_stage.setPName(name.c_str());
        }

        {
            vk::PipelineVertexInputStateCreateInfo vertex_input;
            {
                auto &ci = vertex_input;
                ci.setVertexBindingDescriptionCount(0);
                ci.setVertexBindingDescriptions(nullptr);
                ci.setVertexAttributeDescriptionCount(0);
                ci.setVertexAttributeDescriptions(nullptr);
            }

            vk::PipelineInputAssemblyStateCreateInfo input_assembly;
            {
                auto &ci = input_assembly;
                ci.setTopology(vk::PrimitiveTopology::eTriangleList);
                // allow for triangle strip type tools with special reset indices
                ci.setPrimitiveRestartEnable(VK_FALSE);
            }

            auto ssize = film->swapchain_image_size();
            vk::Extent2D swapchain_extent(ssize.x, ssize.y);
            vk::Rect2D scissor;
            {
                scissor.setOffset({ 0, 0 });
                scissor.setExtent(swapchain_extent);
            }

            vk::Viewport viewport;
            {
                viewport.setX(0.0f);
                viewport.setY(0.0f);
                viewport.setWidth(swapchain_extent.width);
                viewport.setHeight(swapchain_extent.height);
                viewport.setMinDepth(0.0f);
                viewport.setMaxDepth(1.0f);
            }
            vk::PipelineViewportStateCreateInfo viewport_state;
            {
                auto &ci = viewport_state;
                ci.setViewportCount(1);
                ci.setPViewports(&viewport);
                ci.setScissorCount(1);
                ci.setPScissors(&scissor);
            }

            vk::PipelineRasterizationStateCreateInfo rasterization;
            {
                auto &ci = rasterization;
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

            vk::PipelineMultisampleStateCreateInfo multisampling;
            {
                auto &ci = multisampling;
                ci.setSampleShadingEnable(VK_FALSE);
                ci.setRasterizationSamples(vk::SampleCountFlagBits::e1);
                ci.setMinSampleShading(1.0f);
                ci.setPSampleMask(nullptr);
                ci.setAlphaToCoverageEnable(VK_FALSE);
                ci.setAlphaToOneEnable(VK_FALSE);
            }

            vk::PipelineColorBlendAttachmentState color_blend_attachment;
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

            vk::PipelineColorBlendStateCreateInfo color_blend;
            {
                auto &ci = color_blend;
                ci.setLogicOpEnable(VK_FALSE);
                ci.setLogicOp(vk::LogicOp::eCopy);
                ci.setAttachmentCount(1);
                ci.setPAttachments(&color_blend_attachment);
                ci.setBlendConstants({ 0.f, 0.f, 0.f, 0.f });
            }

            std::vector<vk::DynamicState> dynamic_states = {
                vk::DynamicState::eViewport, vk::DynamicState::eLineWidth
            };

            vk::PipelineDynamicStateCreateInfo dynamic_state;
            {
                dynamic_state.setDynamicStateCount(dynamic_states.size());
                dynamic_state.setPDynamicStates(dynamic_states.data());
            }

            {
                vk::PipelineLayoutCreateInfo ci;
                ci.setSetLayoutCount(
                  0);// automatic bindings have cool names, but why
                     // did vulkan not call this layoutCount
                ci.setPSetLayouts(nullptr);
                ci.setPushConstantRangeCount(0);
                ci.setPushConstantRanges(nullptr);
                pipeline_layout = film->device_raii().createPipelineLayout(ci);
            }

            vk::GraphicsPipelineCreateInfo pipeline_info;
            {
                auto &ci = pipeline_info;
                ci.setPRasterizationState(&rasterization);
                ci.setPViewportState(&viewport_state);
                ci.setStageCount(2);
                ci.setPStages(shader_stages);
                ci.setPInputAssemblyState(&input_assembly);
                //ci.setPVertexInputState(&vertex_input);
                //ci.setPMultisampleState(&multisampling);
                //ci.setPDepthStencilState(nullptr);
                //ci.setPColorBlendState(&color_blend);
                //ci.setPDynamicState(nullptr);

                //ci.setLayout(*pipeline_layout);
                //ci.setRenderPass(film->default_render_pass());
                //ci.setSubpass(0);

                //ci.setBasePipelineHandle(VK_NULL_HANDLE);
                //ci.setBasePipelineIndex(-1);
            }

            return;
            pipeline = film->device_raii().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
        }
    }
};


//! [0]
int mainNew(int argc, char *argv[]) {

    glfwInit();

    std::vector<std::string> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_core_validation",
    };
    //! [0]

    try {
        HelloTriangleApplication app;
        //app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    glfwTerminate();

    // fs.make_shader();
    //! [1]

    return 0;
}


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplicationOld {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    std::unique_ptr<GLFWFilmTestShim> film;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        film = std::make_unique<GLFWFilmTestShim>(window);
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }


        glfwDestroyWindow(window);

        glfwTerminate();
    }

    void createInstance() {

        instance = film->instance();
    }


    void setupDebugMessenger() {
        debugMessenger = *film->_debug_messenger_raii;
    }

    void createSurface() {

        surface = film->surface();
    }

    void pickPhysicalDevice() {
        physicalDevice = film->physical_device();
    }

    void createLogicalDevice() {
        device = film->device();

        graphicsQueue = film->graphics_queue();
        presentQueue = film->present_queue();
    }

    void createSwapChain() {



        swapChain = film->swapchain();
        swapChainImageFormat = VkFormat(film->surface_format().format);
        swapChainExtent = VkExtent2D(film->swapchain_extent());

        uint32_t imageCount = film->swapchain_image_count();
        // TODO: prune

        uint32_t swapchain_image_size = film->swapchain_image_count();
        swapChainImages.resize(swapchain_image_size);
        for (int i = 0; i < swapchain_image_size ; i++) {
            swapChainImages[i] = film->swapchain_image(i);

        }
        //spdlog::info("Swapchain_iamge_size: {}", swapchain_image_size);
        //vkGetSwapchainImagesKHR(device, swapChain, &swapchain_image_size, nullptr);
        //vkGetSwapchainImagesKHR(device, swapChain, &swapchain_image_size, swapChainImages.data());
        //spdlog::info("new Swapchain_iamge_size: {}", swapchain_image_size);
    }
    void createImageViewsNew() {
        int swapchain_image_size = film->swapchain_image_count();
        swapChainImageViews.resize(swapchain_image_size);

        for (int i = 0; i < swapchain_image_size ; i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = film->swapchain_image(i);
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createGraphicsPipeline() {
        balsa::visualization::shaders::TriangleShader<balsa::scene_graph::embedding_traits2D> fs;
        auto vert_spv = fs.vert_spirv();
        auto frag_spv = fs.frag_spirv();

        VkShaderModule vertShaderModule = createShaderModule(vert_spv);
        VkShaderModule fragShaderModule = createShaderModule(frag_spv);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pushConstantRangeCount = 0;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) swapChainExtent.width;
            viewport.height = (float) swapChainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }

    }

    void drawFrame() {
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(commandBuffer, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;

        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    VkShaderModule createShaderModule(const std::vector<uint32_t>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    static std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

int mainOld() {
    HelloTriangleApplicationOld app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    mainOld();
}
