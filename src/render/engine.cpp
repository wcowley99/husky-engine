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

  this->init_instance(canvas);

  this->surface =
      vk::UniqueSurfaceKHR(canvas.create_surface(*instance), *instance);

  this->init_device();
  this->allocator =
      std::make_unique<Allocator>(gpu, device.get(), instance.get());

  this->graphics_queue = device->getQueue(queue_family_index, 0);

  this->swapchain =
      std::make_unique<Swapchain>(device.get(), surface.get(), gpu);

  this->frames = std::make_unique<FrameQueue>(device.get(), queue_family_index,
                                              swapchain->num_images());

  this->draw_image =
      ImageBuilder(canvas.extent())
          .with_usage_flags(vk::ImageUsageFlagBits::eTransferSrc |
                            vk::ImageUsageFlagBits::eTransferDst |
                            vk::ImageUsageFlagBits::eStorage |
                            vk::ImageUsageFlagBits::eColorAttachment)
          .with_memory_usage(VMA_MEMORY_USAGE_GPU_ONLY)
          .with_memory_properties(vk::MemoryPropertyFlagBits::eDeviceLocal)
          .build(allocator, device.get());

  this->immediate_command = std::make_unique<ImmediateCommand>(
      device.get(), graphics_queue, queue_family_index);

  this->init_descriptors();
  std::cout << "init_pipelines" << std::endl;
  this->init_pipelines();
  std::cout << "init_buffers" << std::endl;
  this->init_buffers();
  std::cout << "init_imgui" << std::endl;
  this->init_imgui(canvas);
}

Engine::~Engine() {
  device->waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

void Engine::init_instance(Canvas &canvas) {
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
}

void Engine::init_device() {
  auto devices = instance->enumeratePhysicalDevices();

  auto result = select_device(devices);
  this->gpu = result.first;
  this->queue_family_index = result.second;

  this->device = create_device();
  VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
}

void Engine::init_descriptors() {
  this->global_descriptor_allocator =
      DescriptorAllocatorBuilder(device.get())
          .with_descriptor_capacity(vk::DescriptorType::eStorageImage, 10)
          .build();

  this->draw_image_descriptor_layouts =
      DescriptorLayoutBuilder(device.get(), vk::ShaderStageFlagBits::eCompute)
          .add_binding(0, vk::DescriptorType::eStorageImage)
          .build();

  this->draw_image_descriptors = global_descriptor_allocator->allocate(
      draw_image_descriptor_layouts.get());

  vk::DescriptorImageInfo image_info({}, draw_image->image_view.get(),
                                     vk::ImageLayout::eGeneral);

  vk::WriteDescriptorSet write(draw_image_descriptors, 0, {}, 1,
                               vk::DescriptorType::eStorageImage, &image_info);
  device->updateDescriptorSets(1, &write, 0, nullptr);
}

void Engine::init_pipelines() {
  this->gradient_compute =
      ComputePipelineBuilder(device.get(), "gradient2.comp")
          .add_descriptor_layout(draw_image_descriptor_layouts.get())
          .add_push_constant(sizeof(ComputePushConstant))
          .build();

  this->triangle_pipeline =
      GraphicsPipelineBuilder(device.get())
          .with_vertex_shader("colored_triangle.vert")
          .add_push_constant(vk::ShaderStageFlagBits::eVertex,
                             sizeof(MeshPushConstant))
          .with_fragment_shader("colored_triangle.frag")
          .with_topology(vk::PrimitiveTopology::eTriangleList)
          .with_polygon_mode(vk::PolygonMode::eFill)
          .with_cull_mode(vk::CullModeFlagBits::eNone,
                          vk::FrontFace::eClockwise)
          .with_color_attachment_format(draw_image->format)
          .with_depth_attachment_format(vk::Format::eUndefined)
          .build();

  this->mesh_pipeline =
      GraphicsPipelineBuilder(device.get())
          .with_vertex_shader("colored_triangle_mesh.vert")
          .add_push_constant(vk::ShaderStageFlagBits::eVertex,
                             sizeof(MeshPushConstant))
          .with_fragment_shader("colored_triangle.frag")
          .with_topology(vk::PrimitiveTopology::eTriangleList)
          .with_polygon_mode(vk::PolygonMode::eFill)
          .with_cull_mode(vk::CullModeFlagBits::eNone,
                          vk::FrontFace::eClockwise)
          .with_color_attachment_format(draw_image->format)
          .with_depth_attachment_format(vk::Format::eUndefined)
          .build();
}

void Engine::init_buffers() {
  std::vector<Vertex> vertices(4);
  vertices[0].position = {0.5, -0.5, 0};
  vertices[1].position = {0.5, 0.5, 0};
  vertices[2].position = {-0.5, -0.5, 0};
  vertices[3].position = {-0.5, 0.5, 0};

  vertices[0].color = {0, 0, 0, 1};
  vertices[1].color = {0.5, 0.5, 0.5, 1};
  vertices[2].color = {1, 0, 0, 1};
  vertices[3].color = {0, 1, 0, 1};

  std::vector<uint32_t> indices = {0, 1, 2, 2, 1, 3};
  this->mesh_buffer = std::make_unique<MeshBuffer>(
      *allocator, device.get(), *immediate_command, indices, vertices);
}

void Engine::init_imgui(Canvas &canvas) {
  this->imgui_descriptor_allocator =
      DescriptorAllocatorBuilder(device.get())
          .with_descriptor_capacity(vk::DescriptorType::eCombinedImageSampler,
                                    1000)
          .build();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();

  canvas.init_imgui();

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = instance.get();
  init_info.PhysicalDevice = gpu;
  init_info.Device = device.get();
  init_info.QueueFamily = queue_family_index;
  init_info.Queue = graphics_queue;
  init_info.DescriptorPool = imgui_descriptor_allocator->get_pool();
  init_info.MinImageCount = swapchain->num_images();
  init_info.ImageCount = swapchain->num_images();
  init_info.UseDynamicRendering = true;

  auto format = swapchain->image_format();
  vk::PipelineRenderingCreateInfo create_info({}, 1, &format);

  init_info.PipelineRenderingCreateInfo = create_info;
  /*init_info.MSAASamples = vk::SampleCountFlagBits::e1;*/

  ImGui_ImplVulkan_LoadFunctions(
      VK_MAKE_API_VERSION(0, 1, 3, 0),
      [](const char *function_name, void *vulkan_instance) {
        const auto &d = VULKAN_HPP_DEFAULT_DISPATCHER;

        return d.vkGetInstanceProcAddr(
            *(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
      },
      &instance.get());

  ImGui_ImplVulkan_Init(&init_info);
  ImGui_ImplVulkan_CreateFontsTexture();
}

void Engine::draw() {
  auto &current_frame = frames->next();
  VK_ASSERT(device->waitForFences(1, &current_frame.render_fence.get(),
                                  vk::True, 1000000000));

  auto result =
      swapchain->next_image_index(current_frame.swapchain_semaphore.get());

  if (result.result == vk::Result::eErrorOutOfDateKHR) {
    swapchain->recreate();
    return;
  }

  VK_ASSERT(device->resetFences(1, &current_frame.render_fence.get()));

  if (result.result != vk::Result::eSuccess &&
      result.result != vk::Result::eSuboptimalKHR) {
    std::cout << result.result << std::endl;
    throw std::runtime_error("Failed to acquire Next Image");
  }
  auto image_index = result.value;
  auto &swapchain_image = swapchain->image(image_index);

  std::array<float, 4> color = {0.0f, 0.0f,
                                std::abs(std::sin(frame_count / 120.0f)), 1.0f};

  ComputePushConstant pc;
  pc.top_color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
  pc.bottom_color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

  current_frame.command_builder()
      .clear(*draw_image, color)
      .execute_compute(*draw_image, *gradient_compute, draw_image_descriptors,
                       pc)
      .begin_rendering(*draw_image)
      .execute_graphics(*draw_image, *triangle_pipeline)
      .draw_mesh(*draw_image, *mesh_pipeline, *mesh_buffer)
      .end_rendering()
      .copy_to(*draw_image, swapchain_image)
      .draw_imgui(swapchain_image, *swapchain_image.image_view)
      .present(swapchain_image);

  auto present = current_frame.show(*swapchain, graphics_queue, image_index);

  if (present == vk::Result::eErrorOutOfDateKHR ||
      present == vk::Result::eSuboptimalKHR) {
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
    VK_ASSERT(device.getSurfaceSupportKHR(i, *surface, &supported));

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
