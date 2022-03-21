#include "balsa/visualization/vulkan/native_film.hpp"
#include <spdlog/spdlog.h>

#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace {

const std::vector<const char *> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void * /*pUserData*/) {
    spdlog::level::level_enum slevel;
    switch (vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity)) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
        slevel = spdlog::level::trace;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        slevel = spdlog::level::info;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        slevel = spdlog::level::warn;
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        slevel = spdlog::level::err;
        break;
    }
    std::shared_ptr<spdlog::logger> logger = spdlog::get("vulkan");
    if (!bool(logger)) {
        logger = spdlog::default_logger();
    }

    std::string type;
    switch (vk::DebugUtilsMessageTypeFlagBitsEXT(messageType)) {
    case vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral:
        type = "General";
        break;
    case vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance:
        type = "Performance";
        break;
    case vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation:
        type = "Validation";
        break;
    }
    logger->log(slevel, "Vk{}: {}", type, pCallbackData->pMessage);
    return VK_FALSE;
}
}// namespace


namespace balsa::visualization::vulkan {


void NativeFilm::set_device_extensions(const std::vector<std::string> &device_extensions) {
    _device_extensions = device_extensions;
}
void NativeFilm::set_validation_layers(const std::vector<std::string> &validation_layers) {
    _validation_layers = validation_layers;
}

NativeFilm::NativeFilm(const std::vector<std::string> &device_extensions, const std::vector<std::string> &validation_layers) : _device_extensions(device_extensions), _validation_layers(validation_layers) {
    initialize();
}
NativeFilm::NativeFilm(std::nullptr_t) {}

NativeFilm::~NativeFilm() = default;

void NativeFilm::initialize() {
    create_instance();
    create_debug_messenger();
    _surface_raii = make_surface();
    pick_physical_device();
    create_device();
    create_swapchain();
    create_render_pass();
    create_graphics_command_pool();
    create_image_resources();
    create_frame_resources();
    create_render_pass();
}

vk::Instance NativeFilm::instance() const {
    return *_instance_raii;
}
const vk::raii::Instance &NativeFilm::instance_raii() const {
    return _instance_raii;
}

auto NativeFilm::available_queues(
  const vk::PhysicalDevice &device) const -> QueueTargetIndices {
    QueueTargetIndices indices(device);

    return indices;
}
bool NativeFilm::check_validation_layer_support() {

    std::vector<vk::LayerProperties> available_layers = _context_raii.enumerateInstanceLayerProperties();

    for (const auto &_layer_name : _validation_layers) {
        bool layer_found = false;
        std::string_view layer_name{ _layer_name };
        for (const auto &layer_property : available_layers) {
            std::string_view property_layer_name{ layer_property.layerName };
            if (layer_name == property_layer_name) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            spdlog::error("Was unable to load validation layer [{}]",
                          layer_name);
            return false;
        }
    }

    return true;
}
std::vector<std::string> NativeFilm::get_required_extensions() {
    std::vector<std::string> extension_names;
    if (enable_validation_layers) {
        extension_names.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    return extension_names;
}
vk::DebugUtilsMessengerCreateInfoEXT
  NativeFilm::debug_utils_messenger_create_info() const {
    vk::DebugUtilsMessengerCreateInfoEXT create_info;
    create_info.setMessageSeverity(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    create_info.setMessageType(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    create_info.setPfnUserCallback(debugCallback);
    create_info.setPUserData(nullptr);
    return create_info;
}


void NativeFilm::create_device() {
    if (!bool(*_surface_raii)) {
        throw std::runtime_error(
          "Create a surface before creating a device with GLFWFilm");
    }
    QueueTargetIndices indices(*_physical_device_raii, &*_surface_raii);
    // TODO: this is specific to one pipeline - need to support others
    if (indices.graphics_queue.empty()) {
        throw std::runtime_error(
          "Cannot find a graphics queue to make LogicalDevice");
    }
    if (indices.present_queue.empty()) {
        throw std::runtime_error(
          "Cannot find a present queue to make LogicalDevice");
    }

    _graphics_queue_family_index =
      *indices.graphics_queue.begin();
    _present_queue_family_index =
      *indices.present_queue.begin();
    {// try to make graphics and present the same queue. apparently this
     // can be faster
        std::set<uint32_t> mix_queue;
        std::set_intersection(
          indices.graphics_queue.begin(), indices.graphics_queue.end(), indices.present_queue.begin(), indices.present_queue.end(), std::inserter(mix_queue, mix_queue.end()));
        if (!mix_queue.empty()) {
            _graphics_queue_family_index = *mix_queue.begin();
            _present_queue_family_index = _graphics_queue_family_index;
        }
    }
    float queue_priority = 1.0f;
    std::set<uint32_t> final_indices{
        { _graphics_queue_family_index, _present_queue_family_index }
    };
    std::vector<vk::DeviceQueueCreateInfo> dqcis;
    dqcis.reserve(final_indices.size());
    std::transform(final_indices.begin(), final_indices.end(), std::back_inserter(dqcis), [&](uint32_t index) {
        vk::DeviceQueueCreateInfo info;
        info.setQueueFamilyIndex(index);
        info.setQueueCount(1);
        info.setQueuePriorities(queue_priority);
        return info;
    });
    vk::PhysicalDeviceFeatures dev_features = _physical_device_raii.getFeatures();

    vk::DeviceCreateInfo create_info;
    create_info.setPQueueCreateInfos(dqcis.data());
    create_info.setQueueCreateInfoCount(dqcis.size());

    create_info.setEnabledExtensionCount(device_extensions.size());
    create_info.setPpEnabledExtensionNames(device_extensions.data());

    create_info.setPEnabledFeatures(&dev_features);
    std::vector<const char *> names = _validation_layers | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
    if (enable_validation_layers) {
        create_info.setEnabledLayerCount(names.size());
        create_info.setPpEnabledLayerNames(names.data());
    }

    _device_raii = _physical_device_raii.createDevice(create_info);
    _graphics_queue_raii = _device_raii.getQueue(_graphics_queue_family_index, 0);
    _present_queue_raii = _device_raii.getQueue(_present_queue_family_index, 0);
}

void NativeFilm::create_instance() {
    if (enable_validation_layers && !check_validation_layer_support()) {
        throw std::runtime_error(
          "validation layers requirested but not available");
    }

    vk::ApplicationInfo app_info;
    // tutorial says to set it, but vulkan hpp forces this
    assert(app_info.sType == vk::StructureType::eApplicationInfo);
    app_info.setPApplicationName("Hello Triangle");
    app_info.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    app_info.setPEngineName("No Engine");
    app_info.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    app_info.setApiVersion(VK_API_VERSION_1_2);

    vk::InstanceCreateInfo icinfo({}, /*pApplicationInfo=*/&app_info);
    std::vector<const char *> names = _validation_layers | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
    {
        icinfo.setEnabledLayerCount(names.size());
        icinfo.setPpEnabledLayerNames(names.data());
    }

    {
        std::vector<std::string> extension_names = get_required_extensions();
        extension_names.insert(extension_names.end(), device_extensions.begin(), device_extensions.end());

        // for (auto &&extension : extension_names) {
        //     spdlog::info("Require extension {}", extension);
        // }
        std::vector<const char *> names = extension_names | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
        icinfo.setEnabledExtensionCount(names.size());
        icinfo.setPpEnabledExtensionNames(names.data());
    }


    vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
    if (enable_validation_layers) {
        debug_create_info = debug_utils_messenger_create_info();
    }

    icinfo.setPNext(&debug_create_info);
    try {
        _instance_raii = _context_raii.createInstance(icinfo);
    } catch (const std::exception &e) {
        spdlog::error("Error creating instance: {}", e.what());
        throw e;
    }


    std::vector<vk::ExtensionProperties> extensions = _context_raii.enumerateInstanceExtensionProperties();

    for (const auto &extension : extensions) {
        spdlog::info("Instance has extension [{}] available",
                     extension.extensionName);
    }
}

void NativeFilm::create_debug_messenger() {
    // TODO: make sure this extension is only created if available
    if (!enable_validation_layers) {
        return;
    }
    if (!bool(*_instance_raii)) {
        spdlog::error("Cannot create debug messenger without instance set");
    }

    auto create_info = debug_utils_messenger_create_info();
    _debug_messenger_raii = _instance_raii.createDebugUtilsMessengerEXT(create_info);
}
int NativeFilm::score_physical_devices(const vk::PhysicalDevice &device) const {
    // check if we have a graphics pipeline available
    {
        if (!available_queues(device).meets_requirements(
              vk::QueueFlagBits::eGraphics)) {
            return 0;
        }
    }
    int score = 0;
    auto properties = device.getProperties();
    auto features = device.getFeatures();

    switch (properties.deviceType) {
    case vk::PhysicalDeviceType::eDiscreteGpu:
        score += 1000;
        break;
    case vk::PhysicalDeviceType::eIntegratedGpu:
        score += 100;
        break;
    case vk::PhysicalDeviceType::eVirtualGpu:
        score += 10;
        break;
    case vk::PhysicalDeviceType::eCpu:
        score += 1;
        break;
    case vk::PhysicalDeviceType::eOther:
        score += 0;
        break;
    }

    if (!features.geometryShader) {
        spdlog::info("Skipped device from {} due lack of geometry shading",
                     to_string(vk::VendorId(properties.vendorID)));
        return 0;
    }
    // check for complete queues

    return score;
}
void NativeFilm::pick_physical_device() {
    std::vector<vk::raii::PhysicalDevice> devices =
      _instance_raii.enumeratePhysicalDevices();
    if (devices.size() == 0) {
        throw std::runtime_error("No GPU with vulkan support");
    }
    std::multimap<int, vk::PhysicalDevice> candidates;
    for (auto &&device : devices) {
        int score = score_physical_devices(*device);
        auto properties = device.getProperties();
        spdlog::info("physical device: Vendor={} type={} got score {}",
                     to_string(vk::VendorId(properties.vendorID)),
                     to_string(properties.deviceType),
                     score);
        if (score > 0) {
            candidates.emplace(std::make_pair(score, *device));
        }
    }
    candidates.erase(0);
    if (candidates.size() == 0) {
        throw std::runtime_error("Unable to find any suitable candidate GPUs");
    }

    const auto &[score, device] = *candidates.rbegin();
    if (score == 0) {
        throw std::runtime_error("Best candidate GPU is unusable");
    }

    if (!bool(device)) {
        throw std::runtime_error("Unable to find a suitable GPU");
    }
    auto properties = device.getProperties();
    // auto features = device.getFeatures();
    spdlog::info("Chosen physical device: Vendor={} type={}",
                 to_string(vk::VendorId(properties.vendorID)),
                 to_string(properties.deviceType));
    _physical_device_raii = vk::raii::PhysicalDevice(_instance_raii, device);
}


NativeFilm::QueueTargetIndices::QueueTargetIndices(const vk::PhysicalDevice &device,
                                                   vk::SurfaceKHR const *surface) {
    std::vector<vk::QueueFamilyProperties> queue_family_properties =
      device.getQueueFamilyProperties();
    for (auto &&[index, properties] :
         ranges::views::enumerate(queue_family_properties)) {
        if (properties.queueFlags & vk::QueueFlagBits::eGraphics) {
            graphics_queue.emplace(index);
        }
        if (properties.queueFlags & vk::QueueFlagBits::eCompute) {
            compute_queue.emplace(index);
        }
        if (properties.queueFlags & vk::QueueFlagBits::eTransfer) {
            transfer_queue.emplace(index);
        }
        if (properties.queueFlags & vk::QueueFlagBits::eSparseBinding) {
            sparse_binding_queue.emplace(index);
        }
        if (properties.queueFlags & vk::QueueFlagBits::eProtected) {
            protected_queue.emplace(index);
        }
        if (surface != nullptr) {
            VkBool32 supported;
            vkGetPhysicalDeviceSurfaceSupportKHR(
              device, index, static_cast<VkSurfaceKHR>(*surface), &supported);
            if (supported) {
                present_queue.emplace(index);
            }
        }
    }
}

vk::QueueFlags NativeFilm::QueueTargetIndices::active_flags() const {
    vk::QueueFlags flags;

    if (!graphics_queue.empty()) flags |= vk::QueueFlagBits::eGraphics;
    if (!compute_queue.empty()) flags |= vk::QueueFlagBits::eCompute;
    if (!transfer_queue.empty()) flags |= vk::QueueFlagBits::eTransfer;
    if (!sparse_binding_queue.empty())
        flags |= vk::QueueFlagBits::eSparseBinding;
    // if(bool(video_decode_queue)) flags |= vk::QueueFlagBits::eGraphics;
    // if(bool(video_encode_queue)) flags |= vk::QueueFlagBits::eGraphics;
    if (!protected_queue.empty()) flags |= vk::QueueFlagBits::eProtected;
    return flags;
}
bool NativeFilm::QueueTargetIndices::meets_requirements(vk::QueueFlags requirement) const {
    return (requirement & active_flags()) == requirement;
}

glm::uvec2 NativeFilm::swapChainImageSize() const {
    return glm::uvec2(_swapchain_extent.width, _swapchain_extent.height);
}
vk::Format NativeFilm::colorFormat() const {
    return _surface_format.format;
}
vk::CommandBuffer NativeFilm::current_command_buffer() const {
    return *_image_resources.at(_current_swapchain_index).command_buffer_raii;
}
vk::Framebuffer NativeFilm::current_framebuffer() const {
    return *_image_resources.at(_current_swapchain_index).framebuffer_raii;
}
vk::RenderPass NativeFilm::default_render_pass() const {
    return *_default_render_pass_raii;
}
vk::Format NativeFilm::depthStencilFormat() const {
    return vk::Format{};// TODO
}
vk::Image NativeFilm::depthStencilImage() const {
    return nullptr;// TODO:
}
vk::ImageView NativeFilm::depthStencilImageView() const {
    return nullptr;// TODO
}
vk::Device NativeFilm::device() const {
    return *_device_raii;
}
vk::SurfaceKHR NativeFilm::surface() const {
    return *_surface_raii;
}
void NativeFilm::setPhysicalDeviceIndex(int) {
    // TODO
}
vk::PhysicalDevice NativeFilm::physical_device() const {
    return *_physical_device_raii;
}
const vk::raii::PhysicalDevice &NativeFilm::physical_device_raii() const {
    return _physical_device_raii;
}
vk::PhysicalDeviceProperties NativeFilm::physicalDeviceProperties() const {
    return _physical_device_raii.getProperties();
}
vk::CommandPool NativeFilm::graphicsCommandPool() const {
    return *_graphics_command_pool_raii;
}
uint32_t NativeFilm::graphicsQueueFamilyIndex() const {
    return _graphics_queue_family_index;
}
uint32_t NativeFilm::hostVisibleMemoryIndex() const {
    return 0;// TODO
}
vk::Queue NativeFilm::graphicsQueue() const {
    return *_graphics_queue_raii;
}
vk::Image NativeFilm::msaaColorImage(int) const {
    return nullptr;// TODO
}
vk::ImageView NativeFilm::msaaColorImageView(int) const {
    return nullptr;// TODO
}
void NativeFilm::set_sample_count(vk::SampleCountFlagBits) {

    // TODO
}
vk::SampleCountFlagBits NativeFilm::sample_count() const {

    // TODO
    return vk::SampleCountFlagBits::e1;
}
vk::SampleCountFlags NativeFilm::supported_sample_counts() const {

    auto properties = physicalDeviceProperties();
    // TODO
    const auto &limits = properties.limits;
    const auto &color = limits.framebufferColorSampleCounts;
    const auto &depth = limits.framebufferDepthSampleCounts;
    const auto &stencil = limits.framebufferStencilSampleCounts;
    vk::SampleCountFlags flags = color & depth & stencil;
    return flags;
}
int NativeFilm::swapChainImageCount() const {
    return _image_resources.size();
}
vk::Image NativeFilm::swapChainImage(int) const {
    return nullptr;// TODO
}
vk::ImageView NativeFilm::swapChainImageView(int) const {
    return nullptr;// TODO
}

void NativeFilm::create_image_resources() {
    const size_t size = _swapchain_count;


    _image_resources.resize(size);

    vk::CommandBufferAllocateInfo cba_info;
    cba_info.setCommandPool(*_graphics_command_pool_raii);
    cba_info.setLevel(vk::CommandBufferLevel::ePrimary);
    cba_info.setCommandBufferCount(size);
    auto cmd_buffers = _device_raii.allocateCommandBuffers(cba_info);
    auto pres_cmd_buffers = _device_raii.allocateCommandBuffers(cba_info);

    vk::ImageViewCreateInfo img_view_create_info;
    img_view_create_info.setViewType(vk::ImageViewType::e2D);
    img_view_create_info.format = _surface_format.format;
    img_view_create_info.setComponents(vk::ComponentMapping{
      /*.r =*/vk::ComponentSwizzle::eIdentity,
      /*.g =*/vk::ComponentSwizzle::eIdentity,
      /*.b =*/vk::ComponentSwizzle::eIdentity,
      /*.a =*/vk::ComponentSwizzle::eIdentity,
    });

    img_view_create_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    img_view_create_info.subresourceRange.setBaseMipLevel(0);
    img_view_create_info.subresourceRange.setLevelCount(1);
    img_view_create_info.subresourceRange.setBaseArrayLayer(0);
    img_view_create_info.subresourceRange.setLayerCount(1);

    vk::FramebufferCreateInfo framebuffer_ci;
    framebuffer_ci.setRenderPass(*_default_render_pass_raii);
    framebuffer_ci.setAttachmentCount(1);
    framebuffer_ci.setWidth(_swapchain_extent.width);
    framebuffer_ci.setHeight(_swapchain_extent.height);
    framebuffer_ci.setLayers(1);


    auto images = _swapchain_raii.getImages();

    vk::FenceCreateInfo fci;
    fci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (auto &&[res, buf, buf2, img] : ranges::views::zip(_image_resources, cmd_buffers, pres_cmd_buffers, images)) {
        res.command_buffer_raii = std::move(buf);
        res.present_command_buffer_raii = std::move(buf2);
        res.image = img;
        img_view_create_info.setImage(res.image);
        res.image_view_raii = _device_raii.createImageView(img_view_create_info);
        res.command_fence_raii = _device_raii.createFence(fci);
        framebuffer_ci.setPAttachments(&*res.image_view_raii);
        res.framebuffer_raii = _device_raii.createFramebuffer(framebuffer_ci);
    }
}
void NativeFilm::create_frame_resources() {
    _frame_resources.resize(_frame_count);


    vk::FenceCreateInfo fci;
    fci.setFlags(vk::FenceCreateFlagBits::eSignaled);
    for (auto &&res : _frame_resources) {
        res.image_semaphore_raii = _device_raii.createSemaphore({});
        res.draw_semaphore_raii = _device_raii.createSemaphore({});
        res.present_semaphore_raii = _device_raii.createSemaphore({});
        res.fence_raii = _device_raii.createFence(fci);
    }
}
uint32_t NativeFilm::choose_swapchain_image_count() const {
    auto capabilities = _physical_device_raii.getSurfaceCapabilitiesKHR(*_surface_raii);

    uint32_t image_count = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount != 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }
    return image_count;
}

void NativeFilm::create_swapchain() {
    vk::SwapchainCreateInfoKHR create_info;

    create_info.setSurface(*_surface_raii);

    create_info.setImageExtent(_swapchain_extent = choose_swapchain_extent());
    create_info.setMinImageCount(_swapchain_count = choose_swapchain_image_count());// choose format
    {
        auto formats = (*_physical_device_raii).getSurfaceFormatsKHR(*_surface_raii);

        for (const auto &format : formats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                _surface_format = format;
                goto found_format;
            }
        }
        _surface_format = formats[0];
    found_format:

        create_info.setImageFormat(_surface_format.format);
        create_info.setImageColorSpace(_surface_format.colorSpace);
        create_info.setImageArrayLayers(1);
        create_info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);
    }

    {
        vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
        auto modes = (*_physical_device_raii).getSurfacePresentModesKHR(*_surface_raii);
        for (const auto &mode : modes) {
            if (mode == vk::PresentModeKHR::eMailbox) {
                present_mode = mode;
            }
        }
        create_info.setPresentMode(present_mode);
        create_info.clipped = VK_TRUE;
    }

    std::vector<uint32_t> queue_indices;
    queue_indices.emplace_back(_graphics_queue_family_index);
    if (_graphics_queue_family_index != _present_queue_family_index) {
        queue_indices.emplace_back(_present_queue_family_index);
    }

    create_info.setPQueueFamilyIndices(queue_indices.data());
    create_info.setQueueFamilyIndexCount(queue_indices.size());
    if (queue_indices[0] != queue_indices[1]) {
        create_info.setImageSharingMode(vk::SharingMode::eConcurrent);
    } else {
        create_info.setImageSharingMode(vk::SharingMode::eExclusive);
    }
    auto capabilities = _physical_device_raii.getSurfaceCapabilitiesKHR(*_surface_raii);
    create_info.setPreTransform(capabilities.currentTransform);

    create_info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);

    create_info.setOldSwapchain(VK_NULL_HANDLE);
    _swapchain_raii = _device_raii.createSwapchainKHR(create_info);
}


void NativeFilm::create_render_pass() {
    vk::AttachmentDescription color_attachment;
    color_attachment.setFormat(_surface_format.format);
    color_attachment.setSamples(vk::SampleCountFlagBits::e1);

    color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);

    color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
    color_attachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.setAttachment(0);
    color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(1);
    subpass.setPColorAttachments(&color_attachment_ref);

    vk::RenderPassCreateInfo create_info;
    create_info.setAttachmentCount(1);
    create_info.setPAttachments(&color_attachment);
    create_info.setSubpassCount(1);
    create_info.setPSubpasses(&subpass);

    vk::SubpassDependency dep;
    dep.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dep.setDstSubpass(0);

    dep.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dep.setSrcAccessMask(vk::AccessFlags(0));
    dep.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dep.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    create_info.setDependencies(dep);
    _default_render_pass_raii = _device_raii.createRenderPass(create_info);
}
void NativeFilm::create_graphics_command_pool() {

    vk::CommandPoolCreateInfo ci;
    ci.setQueueFamilyIndex(_graphics_queue_family_index);
    ci.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    _graphics_command_pool_raii = _device_raii.createCommandPool(ci);
}


void NativeFilm::pre_draw() {

    auto device = this->device();
    auto &frame = _frame_resources[_current_frame_index];

    vk::Result result;

    if (!frame.image_acquired) {
        if (frame.fence_waitable) {
            result = device.waitForFences(*frame.fence_raii, VK_TRUE, UINT64_MAX);


            vk::ResultValue<uint32_t> res =
              device.acquireNextImageKHR(*_swapchain_raii, UINT64_MAX, *frame.image_semaphore_raii, VK_NULL_HANDLE);
            if (res.result == vk::Result::eSuccess) {
                _current_swapchain_index = res.value;

                frame.image_semaphore_waitable = true;
                frame.image_acquired = true;
                frame.fence_waitable = true;
            } else {// TODO if device is lost or some other issue occurs
                spdlog::error("Failed to acquire image");
            }
        }
    }

    auto &image = _image_resources[_current_swapchain_index];

    if (image.command_fence_waitable) {
        result = device.waitForFences(*image.command_fence_raii, VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Fence failed");
        }
        device.resetFences(*image.command_fence_raii);
    }


    vk::CommandBufferAllocateInfo ci;
    ci.setCommandPool(*_graphics_command_pool_raii);
    ci.setLevel(vk::CommandBufferLevel::ePrimary);
    ci.setCommandBufferCount(1);
    image.command_buffer_raii = std::move(_device_raii.allocateCommandBuffers(ci)[0]);
}

void NativeFilm::post_draw() {
    auto &image = _image_resources[_current_swapchain_index];
    auto &frame = _frame_resources[_current_frame_index];


    // TODO: swapchain image release to cmd buf to send to graphics queue
    // TODO: add a readback?
    //
    image.command_buffer_raii.endRenderPass();

    image.command_buffer_raii.end();

    vk::SubmitInfo si;
    vk::PipelineStageFlags wait_stages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };
    {
        si.setWaitSemaphores(*frame.image_semaphore_raii);

        // if frame grabbing?
        if (true) {
            si.setSignalSemaphores(*frame.draw_semaphore_raii);
        }
        si.setWaitDstStageMask(*wait_stages);
        si.setCommandBuffers(*image.command_buffer_raii);
    }

    _graphics_queue_raii.submit(si, *image.command_fence_raii);
    frame.image_semaphore_waitable = false;
    image.command_fence_waitable = true;

    if (_present_queue_family_index != _graphics_queue_family_index) {
        si.setWaitSemaphores(*frame.draw_semaphore_raii);
        si.setSignalSemaphores(*frame.present_semaphore_raii);
        si.setCommandBuffers(*image.present_command_buffer_raii);
        _present_queue_raii.submit(si, *image.command_fence_raii);
    }

    vk::PresentInfoKHR present;
    present.setWaitSemaphores(*frame.present_semaphore_raii);

    present.setSwapchains(*_swapchain_raii);
    present.setImageIndices(_current_swapchain_index);

    present.setPResults(nullptr);
    vk::Result result =
      _present_queue_raii.presentKHR(present);
    if (result != vk::Result::eSuccess) {
        spdlog::error("Failed to present");
    }
}
}// namespace balsa::visualization::vulkan
