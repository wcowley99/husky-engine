#include "engine.h"

#include <cmath>
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

  this->device = create_device();
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

  this->graphics_queue = device->getQueue(queue_family_index, 0);

  this->swapchain = Swapchain::create(device.get(), surface.get(), gpu);

  vk::CommandPoolCreateInfo command_pool_info(
      vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index);

  vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled);
  vk::SemaphoreCreateInfo semaphore_info;
  for (auto &frame_data : frames) {
    frame_data.pool = device->createCommandPoolUnique(command_pool_info);
    vk::CommandBufferAllocateInfo alloc_info(
        frame_data.pool.get(), vk::CommandBufferLevel::ePrimary, 1);
    frame_data.buffer = device->allocateCommandBuffers(alloc_info)[0];
    frame_data.render_fence = device->createFenceUnique(fence_info);
    frame_data.swapchain_semaphore =
        device->createSemaphoreUnique(semaphore_info);
    frame_data.render_semaphore = device->createSemaphoreUnique(semaphore_info);
  }
}

Engine::~Engine() {
  device->waitIdle();
  for (auto &frame : frames) {
    device->destroySemaphore(frame.render_semaphore.release(), nullptr);
    device->destroySemaphore(frame.swapchain_semaphore.release(), nullptr);
    device->destroyFence(frame.render_fence.release());
    device->destroyCommandPool(frame.pool.release());
  }
}

void Engine::draw() {
  auto &current_frame = get_current_frame();
  vk_assert(device->waitForFences(1, &current_frame.render_fence.get(),
                                  vk::True, 1000000000));

  auto result =
      swapchain->next_image_index(current_frame.swapchain_semaphore.get());

  if (result.result == vk::Result::eErrorOutOfDateKHR) {
    swapchain->recreate();
    return;
  }

  vk_assert(device->resetFences(1, &current_frame.render_fence.get()));

  if (result.result != vk::Result::eSuccess &&
      result.result != vk::Result::eSuboptimalKHR) {
    std::cout << result.result << std::endl;
    throw std::runtime_error("Failed to acquire Next Image: ");
  }
  auto image_index = result.value;
  auto swapchain_image = swapchain->image(image_index);

  current_frame.buffer.reset();
  vk::CommandBufferBeginInfo begin_info(
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  current_frame.buffer.begin(begin_info);

  transition_image(current_frame.buffer, swapchain_image,
                   vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

  std::array<float, 4> colors = {
      0.0f, 0.0f, std::abs(std::sin(frame_count / 120.0f)), 1.0f};
  vk::ClearColorValue color(colors);

  vk::ImageSubresourceRange clear_range(vk::ImageAspectFlagBits::eColor, 0,
                                        vk::RemainingMipLevels, 0,
                                        vk::RemainingArrayLayers);
  current_frame.buffer.clearColorImage(
      swapchain_image, vk::ImageLayout::eGeneral, &color, 1, &clear_range);
  transition_image(current_frame.buffer, swapchain_image,
                   vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR);

  current_frame.buffer.end();

  vk::CommandBufferSubmitInfo submit_info(current_frame.buffer);
  vk::SemaphoreSubmitInfo wait_info(
      current_frame.swapchain_semaphore.get(), 1,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  vk::SemaphoreSubmitInfo signal_info(current_frame.render_semaphore.get(), 1,
                                      vk::PipelineStageFlagBits2::eAllGraphics);
  vk::SubmitInfo2 submit({}, 1, &wait_info, 1, &submit_info, 1, &signal_info);
  if (graphics_queue.submit2(1, &submit, current_frame.render_fence.get()) !=
      vk::Result::eSuccess) {
    throw std::runtime_error("Failed Submit2");
  }

  auto present = swapchain->get_present_info(
      &current_frame.render_semaphore.get(), &image_index);
  auto present_result = graphics_queue.presentKHR(present);
  if (present_result == vk::Result::eErrorOutOfDateKHR ||
      present_result == vk::Result::eSuboptimalKHR) {
    swapchain->recreate();
  }

  frame_count += 1;
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

    vk::Bool32 supported;
    vk_assert(device.getSurfaceSupportKHR(i, *surface, &supported));

    if (prop.queueFlags & vk::QueueFlagBits::eGraphics && supported) {
      return i;
    }
  }

  return {};
}

vk::UniqueDevice Engine::create_device() {
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

  return gpu.createDeviceUnique(
      vk::DeviceCreateInfo({}, 1, &queue_create_info, 0, nullptr,
                           extensions.size(), extensions.data())
          .setPNext(&features));
}

} // namespace Render
