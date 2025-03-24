#include "engine.h"

#include <iostream>

namespace Render {

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
               void *pUserData) {
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE;
}

Engine::Engine(Canvas &canvas) {
  VULKAN_HPP_DEFAULT_DISPATCHER.init();

  vk::ApplicationInfo app_info("Hello, World", VK_MAKE_VERSION(1, 0, 0),
                               "No Engine", VK_MAKE_VERSION(1, 0, 0),
                               VK_API_VERSION_1_3);

  auto instance_extensions = canvas.required_instance_extensions();
  instance_extensions.push_back("VK_EXT_debug_utils");
  instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

  auto available_layers = vk::enumerateInstanceLayerProperties();
  for (auto layer : available_layers) {
    std::cout << layer.layerName << std::endl;
  }

  auto layers = std::vector<const char *>{"VK_LAYER_KHRONOS_validation"};

  this->instance = vk::createInstanceUnique(vk::InstanceCreateInfo{
      vk::InstanceCreateFlags{VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR},
      &app_info, (uint32_t)layers.size(), layers.data(),
      (uint32_t)instance_extensions.size(), instance_extensions.data()});

  VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

  auto messenger = instance->createDebugUtilsMessengerEXTUnique(
      vk::DebugUtilsMessengerCreateInfoEXT{
          {},
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
          vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
          debug_callback},
      nullptr);

  this->surface =
      vk::UniqueSurfaceKHR(canvas.create_surface(*instance), *instance);

  auto devices = instance->enumeratePhysicalDevices();

  auto result = select_device(devices);
  this->gpu = result.first;
  this->queue_family_index = result.second;

  this->device = vk::UniqueDevice(create_device());
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
}

std::pair<vk::PhysicalDevice, uint32_t>
Engine::select_device(const std::vector<vk::PhysicalDevice> &devices) const {
  for (auto device : devices) {
    if (!has_required_vulkan(device) || !has_required_features(device)) {
      continue;
    }

    auto queue_family_index = find_suitable_queue_family(device);

    if (queue_family_index.has_value()) {
      std::cerr << "=== Selected Device: " << device.getProperties().deviceName
                << std::endl;
      return {device, queue_family_index.value()};
    }
  }

  throw std::runtime_error("Could not find a suitable Device.");
}

bool Engine::has_required_vulkan(vk::PhysicalDevice device) const {
  auto props = device.getProperties();

  return props.apiVersion >= VK_API_VERSION_1_3;
}

bool Engine::has_required_features(vk::PhysicalDevice device) const {
  auto features = device.getFeatures2<vk::PhysicalDeviceFeatures2,
                                      vk::PhysicalDeviceVulkan12Features,
                                      vk::PhysicalDeviceVulkan13Features>();
  auto features12 = features.get<vk::PhysicalDeviceVulkan12Features>();
  auto features13 = features.get<vk::PhysicalDeviceVulkan13Features>();

  return features12.bufferDeviceAddress && features12.descriptorIndexing &&
         features13.dynamicRendering && features13.synchronization2;
}

std::optional<uint32_t>
Engine::find_suitable_queue_family(vk::PhysicalDevice device) const {
  auto props = device.getQueueFamilyProperties();

  for (uint32_t i = 0; i < props.size(); i += 1) {
    auto prop = props[i];
    std::cerr << " === [" << i << "]: Graphics: "
              << (uint32_t)(prop.queueFlags & vk::QueueFlagBits::eGraphics)
              << std::endl;
    vk::Bool32 supported;
    vk_assert(device.getSurfaceSupportKHR(i, *surface, &supported));

    std::cerr << " === [" << i << "]: Surface: " << supported << std::endl;

    if (prop.queueFlags & vk::QueueFlagBits::eGraphics && supported) {
      return i;
    }
  }

  return {};
}

vk::Device Engine::create_device() {
  std::vector<const char *> extensions = {vk::KHRSwapchainExtensionName};

  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan12Features,
                     vk::PhysicalDeviceVulkan13Features>
      c;

  auto &features12 = c.get<vk::PhysicalDeviceVulkan12Features>();

  features12.setBufferDeviceAddress(vk::True);
  features12.setDescriptorIndexing(vk::True);

  auto &features13 = c.get<vk::PhysicalDeviceVulkan13Features>();

  features13.setDynamicRendering(vk::True);
  features13.setSynchronization2(vk::True);

  auto features = c.get();

  float priority = 1.0f;
  auto queue_create_info =
      vk::DeviceQueueCreateInfo({}, queue_family_index, 1, &priority);

  return gpu.createDevice(vk::DeviceCreateInfo({}, 1, &queue_create_info, 0,
                                               nullptr, extensions.size(),
                                               extensions.data())
                              .setPNext(&features));
}

} // namespace Render
