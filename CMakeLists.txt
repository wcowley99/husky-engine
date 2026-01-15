cmake_minimum_required(VERSION 3.21)

project(
  civ-game
  VERSION 0.0.1
  LANGUAGES C CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Source
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)

add_subdirectory(external/volk)

add_library(VulkanMemoryAllocator 
  external/VulkanMemoryAllocator/src/VmaUsage.cpp
)

target_include_directories(VulkanMemoryAllocator PRIVATE
  external/VulkanMemoryAllocator/include
)

target_link_libraries(VulkanMemoryAllocator PRIVATE 
  Vulkan::Headers
)

target_compile_definitions(VulkanMemoryAllocator PUBLIC 
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYANMIC_VULKAN_FUNCTIONS=0
)

add_library(engine-lib
  src/model/model.c

  src/renderer/buffer.c
  src/renderer/command.c
  src/renderer/descriptors.c
  src/renderer/image.c
  src/renderer/material.c
  src/renderer/pipeline.c
  src/renderer/renderer.c
  src/renderer/vkb.c

  src/common/array.c
  src/common/linalgebra.c
  src/common/str.c
  src/common/util.c
)

target_link_libraries(engine-lib PUBLIC
  SDL3::SDL3
  Vulkan::Headers
  VulkanMemoryAllocator
  volk
)

target_include_directories(engine-lib PUBLIC
  external/VulkanMemoryAllocator/include
  external/volk
  external/stb
  ${CMAKE_BINARY_DIR}

  src
)

# Build main
add_executable(civ-game src/main.c)
target_link_libraries(civ-game engine-lib)

# Build Test
add_executable(test test/test.c)
target_link_libraries(test engine-lib)
target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR})
