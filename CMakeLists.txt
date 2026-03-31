cmake_minimum_required(VERSION 3.21)

project(
  civ-game
  VERSION 0.0.1
  LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Source
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)

add_subdirectory(external)

add_library(engine-lib
  src/model/model.c

  src/renderer/buffer.c
  src/renderer/command.c
  src/renderer/descriptors.c
  src/renderer/image.c
  src/renderer/pipeline.c
  src/renderer/platform.c
  src/renderer/renderer.c
  src/renderer/swapchain.c
  src/renderer/vk_context.c
  src/renderer/vkb.c

  src/common/array.c
  src/common/str.c
  src/common/util.c

  src/scene/camera.c
  src/scene/scene.c
)

target_link_libraries(engine-lib PUBLIC
  SDL3::SDL3
  Vulkan::Headers
  volk

  # Vulkan Memory Allocator
  external
)

target_include_directories(engine-lib PUBLIC
  external/VulkanMemoryAllocator/include
  external/volk
  external/stb
  external/cgltf
  external/cglm/include
  external/assimp/include
  ${CMAKE_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/external/assimp/include

  src
  include
)

# Build main
add_executable(civ-game src/main.c)
target_link_libraries(civ-game engine-lib)
