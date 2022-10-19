#include "balsa/visualization/vulkan/native_film.hpp"
#include <spdlog/spdlog.h>

#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace {


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
        slevel = spdlog::level::trace;
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

    std::string_view type;
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
void NativeFilm::set_instance_extensions(const std::vector<std::string> &instance_extensions) {
    _instance_extensions = instance_extensions;
}
void NativeFilm::set_validation_layers(const std::vector<std::string> &validation_layers) {
    _validation_layers = validation_layers;
}

NativeFilm::NativeFilm(const std::vector<std::string> &device_extensions, const std::vector<std::string> &instance_extensions, const std::vector<std::string> &validation_layers) : _device_extensions(device_extensions), _instance_extensions(instance_extensions), _validation_layers(validation_layers) {
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

const vk::SwapchainKHR &NativeFilm::swapchain() const {
    return *_swapchain_raii;
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
std::vector<std::string> NativeFilm::get_required_instance_extensions() const {
    std::vector<std::string> extension_names;
    if (enable_validation_layers) {
        extension_names.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    return extension_names;
}

std::vector<std::string> NativeFilm::get_required_device_extensions() const {
    std::vector<std::string> extension_names;
    extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

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
        vk::DeviceQueueCreateInfo trace;
        trace.setQueueFamilyIndex(index);
        trace.setQueueCount(1);
        trace.setQueuePriorities(queue_priority);
        return trace;
    });
    vk::PhysicalDeviceFeatures dev_features = _physical_device_raii.getFeatures();

    vk::DeviceCreateInfo create_info;
    create_info.setPQueueCreateInfos(dqcis.data());
    create_info.setQueueCreateInfoCount(dqcis.size());

    std::vector<std::string> extension_names = get_required_device_extensions();
    std::vector<const char *> ext_cnames;
    {
        extension_names.insert(extension_names.end(), _device_extensions.begin(), _device_extensions.end());

        // for (auto &&extension : extension_names) {
        //     spdlog::trace("Require extension {}", extension);
        // }
        ext_cnames = extension_names | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
        create_info.setEnabledExtensionCount(ext_cnames.size());
        create_info.setPpEnabledExtensionNames(ext_cnames.data());
    }


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

    vk::ApplicationInfo app_trace;
    // tutorial says to set it, but vulkan hpp forces this
    assert(app_trace.sType == vk::StructureType::eApplicationInfo);
    app_trace.setPApplicationName("Hello Triangle");
    app_trace.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    app_trace.setPEngineName("No Engine");
    app_trace.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    app_trace.setApiVersion(VK_API_VERSION_1_2);

    vk::InstanceCreateInfo ictrace({}, /*pApplicationInfo=*/&app_trace);
    std::vector<const char *> layer_cnames = _validation_layers | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
    {
        ictrace.setEnabledLayerCount(layer_cnames.size());
        ictrace.setPpEnabledLayerNames(layer_cnames.data());
    }

    std::vector<std::string> extension_names = get_required_instance_extensions();
    std::vector<const char *> ext_cnames;
    {
        extension_names.insert(extension_names.end(), _instance_extensions.begin(), _instance_extensions.end());

        // for (auto &&extension : extension_names) {
        //     spdlog::trace("Require extension {}", extension);
        // }
        ext_cnames = extension_names | ranges::views::transform([](const std::string &str) -> const char * { return str.c_str(); }) | ranges::to_vector;
        ictrace.setEnabledExtensionCount(ext_cnames.size());
        ictrace.setPpEnabledExtensionNames(ext_cnames.data());
    }


    vk::DebugUtilsMessengerCreateInfoEXT debug_create_info;
    if (enable_validation_layers) {
        debug_create_info = debug_utils_messenger_create_info();
        ictrace.setPNext(&debug_create_info);
    }
    for (auto &&ext : extension_names) {
        spdlog::trace("str ext: {}", ext);
    }
    for (auto &&ext : extension_names) {
        spdlog::trace("str ext: {}", ext);
    }
    for (auto &&ext : _validation_layers) {
        spdlog::trace("str layer: {}", ext);
    }
    for (auto &&ext : ext_cnames) {
        spdlog::trace("ext: {}", ext);
    }
    for (auto &&ext : layer_cnames) {
        spdlog::trace("layer: {}", ext);
    }

    try {
        _instance_raii = _context_raii.createInstance(ictrace);
    } catch (const std::exception &e) {
        spdlog::error("Error creating instance: {}", e.what());

        std::vector<vk::ExtensionProperties> extensions = _context_raii.enumerateInstanceExtensionProperties();

        for (const auto &extension : extensions) {
            spdlog::trace("Instance has extension [{}] available",
                         extension.extensionName);
        }
        auto layer_properties = _context_raii.enumerateInstanceLayerProperties();

        for (const auto &layer : layer_properties) {
            spdlog::trace("Instance has layer [{}] available",
                         layer.layerName);
        }
        throw e;
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
        spdlog::trace("Skipped device from {} due lack of geometry shading",
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
        spdlog::trace("physical device: Vendor={} type={} got score {}",
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
    spdlog::trace("Chosen physical device: Vendor={} type={}",
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

glm::uvec2 NativeFilm::swapchain_image_size() const {
    return glm::uvec2(_swapchain_extent.width, _swapchain_extent.height);
}
vk::Format NativeFilm::color_format() const {
    return _surface_format.format;
}
vk::CommandBuffer NativeFilm::current_command_buffer() const {
    return *_image_resources.at(_current_image_index).command_buffer_raii;
}
vk::Framebuffer NativeFilm::current_framebuffer() const {
    return *_image_resources.at(_current_image_index).framebuffer_raii;
}
vk::RenderPass NativeFilm::default_render_pass() const {
    return *_default_render_pass_raii;
}
vk::Format NativeFilm::depth_stencil_format() const {
    return vk::Format{};// TODO
}
vk::Image NativeFilm::depth_stencil_image() const {
    return nullptr;// TODO:
}
vk::ImageView NativeFilm::depth_stencil_image_view() const {
    return nullptr;// TODO
}
vk::Device NativeFilm::device() const {
    return *_device_raii;
}
const vk::raii::Device &NativeFilm::device_raii() const {
    return _device_raii;
}
vk::SurfaceKHR NativeFilm::surface() const {
    return *_surface_raii;
}
void NativeFilm::set_physical_device_index(int) {
    // TODO
}
vk::PhysicalDevice NativeFilm::physical_device() const {
    return *_physical_device_raii;
}
const vk::raii::PhysicalDevice &NativeFilm::physical_device_raii() const {
    return _physical_device_raii;
}
vk::PhysicalDeviceProperties NativeFilm::physical_device_properties() const {
    return _physical_device_raii.getProperties();
}
vk::CommandPool NativeFilm::graphics_command_pool() const {
    return *_graphics_command_pool_raii;
}
uint32_t NativeFilm::graphics_queue_family_index() const {
    return _graphics_queue_family_index;
}
uint32_t NativeFilm::present_queue_family_index() const {
    return _present_queue_family_index;
}
uint32_t NativeFilm::host_visible_memory_index() const {
    return 0;// TODO
}
vk::Queue NativeFilm::graphics_queue() const {
    return *_graphics_queue_raii;
}
vk::Queue NativeFilm::present_queue() const {
    return *_present_queue_raii;
}
vk::Image NativeFilm::msaa_color_image(int) const {
    return nullptr;// TODO
}
vk::ImageView NativeFilm::msaa_color_image_view(int) const {
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

    auto properties = physical_device_properties();
    // TODO
    const auto &limits = properties.limits;
    const auto &color = limits.framebufferColorSampleCounts;
    const auto &depth = limits.framebufferDepthSampleCounts;
    const auto &stencil = limits.framebufferStencilSampleCounts;
    vk::SampleCountFlags flags = color & depth & stencil;
    return flags;
}
uint32_t NativeFilm::swapchain_image_count() const {
    return _image_resources.size();
}
vk::Image NativeFilm::swapchain_image(int index) const {
    return _image_resources.at(index).image;
}
vk::ImageView NativeFilm::swapchain_image_view(int index) const {
    return *_image_resources.at(index).image_view_raii;
}
    const vk::Extent2D& NativeFilm::swapchain_extent() const
{
    return _swapchain_extent;
}
    const vk::SurfaceFormatKHR NativeFilm::surface_format() const
{
    return _surface_format;
}

void NativeFilm::create_image_resources() {

    auto images = _swapchain_raii.getImages();
    if(_swapchain_count != images.size())
    {
        spdlog::error("Swapchain requested more images than we requested: {} vs {}", images.size(), _swapchain_count);
    }
    const size_t size = images.size();

    _image_resources.resize(size);

    vk::CommandBufferAllocateInfo cba_trace;
    cba_trace.setCommandPool(*_graphics_command_pool_raii);
    cba_trace.setLevel(vk::CommandBufferLevel::ePrimary);
    cba_trace.setCommandBufferCount(size);
    auto cmd_buffers = _device_raii.allocateCommandBuffers(cba_trace);
    auto pres_cmd_buffers = _device_raii.allocateCommandBuffers(cba_trace);

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
    spdlog::error("Desired swapchain image count: {}", image_count);
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
    spdlog::trace("pre_draw: using frame {}/{}", _current_frame_index, _frame_resources.size());

    vk::Result result;

    if (!frame.image_acquired) {
        if (frame.fence_waitable) {
            spdlog::trace("pre_draw: waiting for fence_raii");
            result = device.waitForFences(*frame.fence_raii, VK_TRUE, UINT64_MAX);
            device.resetFences(*frame.fence_raii);
            frame.fence_waitable = false;
        }


        spdlog::trace("pre_draw: acquiring image");
        vk::ResultValue<uint32_t> res =
            device.acquireNextImageKHR(*_swapchain_raii, UINT64_MAX, *frame.image_semaphore_raii, *frame.fence_raii);
        _current_image_index = res.value;
        if (res.result == vk::Result::eSuccess || res.result == vk::Result::eSuboptimalKHR) {
            spdlog::trace("Acquired image {}", res.value);
            frame.fence_waitable = true;
            frame.image_semaphore_waitable = true;
            frame.image_acquired = true;
        } else if (res.result == vk::Result::eErrorOutOfDateKHR) {
            // recreate swapchains
        } else {// TODO if device is lost or some other issue occurs
            spdlog::error("Failed to acquire image");
        }
    }
    spdlog::trace("pre_draw: using image {}/{}", _current_image_index, _image_resources.size());

    auto &image = _image_resources[_current_image_index];

    if (image.command_fence_waitable) {
        spdlog::trace("pre_draw: waiting for command fence");
        result = device.waitForFences(*image.command_fence_raii, VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Fence failed");
        }
        spdlog::trace("pre_draw: Resetting command fence");
        device.resetFences(*image.command_fence_raii);
        image.command_fence_waitable = false;
    } else {
        spdlog::warn("Image command fence was not waitable");
    }


    image.command_buffer_raii.reset();
    {
        vk::CommandBufferBeginInfo bi;
        {
            bi.setFlags({});// not used
            bi.setPInheritanceInfo(nullptr);
        }
        image.command_buffer_raii.begin(bi);
    }
    //if (false) {

    //    auto cb = current_command_buffer();
    //    vk::ClearValue clear_color =
    //      vk::ClearColorValue{ std::array<float, 4>{ 1.f, 0.f, 0.f, 1.f } };
    //    vk::RenderPassBeginInfo rpi;
    //    {
    //        rpi.setRenderPass(default_render_pass());
    //        rpi.setFramebuffer(current_framebuffer());
    //        rpi.renderArea.setOffset({ 0, 0 });
    //        auto extent = swapchain_image_size();
    //        rpi.renderArea.setExtent({ extent.x, extent.y });

    //        rpi.setClearValueCount(1);
    //        rpi.setPClearValues(&clear_color);
    //    }

    //    cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

    //    cb.endRenderPass();
    //}

    //
}

void NativeFilm::post_draw() {
    auto &image = _image_resources[_current_image_index];
    auto &frame = _frame_resources[_current_frame_index];

    spdlog::trace("post_draw: using frame {}/{}", _current_frame_index, _frame_resources.size());
    spdlog::trace("post_draw: using image {}/{}", _current_image_index, _image_resources.size());

    if (_graphics_queue_family_index != _present_queue_family_index) {
        spdlog::trace("Transfer barrier");
        vk::ImageMemoryBarrier presentTransfer;
        presentTransfer.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
        presentTransfer.setOldLayout(vk::ImageLayout::ePresentSrcKHR);
        presentTransfer.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
        presentTransfer.setSrcQueueFamilyIndex(_graphics_queue_family_index);
        presentTransfer.setDstQueueFamilyIndex(_present_queue_family_index);
        presentTransfer.setImage(image.image);
        presentTransfer.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, {}, 1, {}, 1));
        image.command_buffer_raii.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, presentTransfer);
    }

    // TODO: swapchain image release to cmd buf to send to graphics queue
    // TODO: add a readback?
    //
    //image.command_buffer_raii.endRenderPass();

    image.command_buffer_raii.end();

    spdlog::trace("post_draw: preparing submit trace");
    vk::SubmitInfo si;
    {
        if (frame.image_semaphore_waitable) {
            spdlog::trace("image -> draw semaphores");
            spdlog::trace("image semaphore raii {}", bool(*frame.image_semaphore_raii));
            si.setWaitSemaphores(*frame.image_semaphore_raii);
        }

        // if frame grabbing?
        if (true) {
            spdlog::trace("draw semaphore raii {}", bool(*frame.draw_semaphore_raii));
            si.setSignalSemaphores(*frame.draw_semaphore_raii);
        }
        vk::PipelineStageFlags flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        si.setWaitDstStageMask(flags);
        si.setCommandBuffers(*image.command_buffer_raii);
    }

    try {
        spdlog::trace("post_draw: adding image fence to graphics queue with command_fence_raii");
        spdlog::trace("command fence raii {}", bool(*image.command_fence_raii));
        _graphics_queue_raii.submit(si, *image.command_fence_raii);
        frame.image_semaphore_waitable = false;
        image.command_fence_waitable = true;
    } catch (const std::exception &e) {
        spdlog::error("Failed: {}", e.what());
    }

    const bool graphics_present_different_familes = _present_queue_family_index != _graphics_queue_family_index;
    if (graphics_present_different_familes) {
        spdlog::trace("post_draw: attaching semaphores because queues are different (graphics={}, present={})", _graphics_queue_family_index, _present_queue_family_index);
        spdlog::trace("draw -> present semaphores");
        si.setWaitSemaphores(*frame.draw_semaphore_raii);
        si.setSignalSemaphores(*frame.present_semaphore_raii);
        si.setCommandBuffers(*image.present_command_buffer_raii);
        _present_queue_raii.submit(si, *image.command_fence_raii);
    }

    spdlog::trace("post_draw: setting present wait semaphore");
    vk::PresentInfoKHR present;
    present.setSwapchains(*_swapchain_raii);
    present.setImageIndices(_current_image_index);

    // present.setPResults(nullptr);

    if (graphics_present_different_familes) {
        spdlog::trace("present-> done");
        present.setWaitSemaphores(*frame.present_semaphore_raii);
    } else {
        spdlog::trace("draw-> done");
        present.setWaitSemaphores(*frame.draw_semaphore_raii);
    }

    spdlog::trace("post_draw: calling present");
    vk::Result result =
      _present_queue_raii.presentKHR(present);
    if (result != vk::Result::eSuccess) {
        spdlog::error("Failed to present");
    }
    frame.image_acquired = false;
    _current_frame_index = (_current_frame_index + 1) % _frame_resources.size();
    spdlog::trace("post_draw: Done");
}
}// namespace balsa::visualization::vulkan
