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


const std::vector<const char *> NativeFilm::validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};


NativeFilm::NativeFilm() {
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
    // create_swapchain();
    // create_swapchain_image_views();
    // create_render_pass();
    // create_graphics_pipeline();
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

    for (const auto &_layer_name : validation_layers) {
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
std::vector<const char *> NativeFilm::get_required_extensions() {
    std::vector<const char *> extension_names;
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
    if (enable_validation_layers) {
        create_info.setEnabledLayerCount(validation_layers.size());
        create_info.setPpEnabledLayerNames(validation_layers.data());
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
    icinfo.setEnabledLayerCount(validation_layers.size());
    icinfo.setPpEnabledLayerNames(validation_layers.data());

    std::vector<std::string> extension_names = get_required_extensions();
    // for (auto &&extension : extension_names) {
    //     spdlog::info("Require extension {}", extension);
    // }
    std::vector<const char*> names = extension_names | ranges::views::transform([](const std::string& str) -> const char* { return str.c_str(); }) | ranges::to_vector;
    icinfo.setEnabledExtensionCount(names.size());
    icinfo.setPpEnabledExtensionNames(names.data());

    // TODO: why is this 0?
    icinfo.setEnabledLayerCount(0);

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
vk::CommandBuffer NativeFilm::currentCommandBuffer() const {
    return _image_resources.at(_current_swapchain_index).command_buffer;
}
vk::Framebuffer NativeFilm::currentFramebuffer() const {
    return *_image_resources.at(_current_swapchain_index).framebuffer_raii;
}
vk::RenderPass NativeFilm::defaultRenderPass() const {
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
void NativeFilm::setPhysicalDeviceIndex(int) {
    // TODO
}
vk::PhysicalDevice NativeFilm::physicalDevice() const {
    return *_physical_device_raii;
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

}// namespace balsa::visualization::vulkan
