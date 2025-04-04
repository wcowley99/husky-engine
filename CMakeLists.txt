cmake_minimum_required(VERSION 3.20)

project(
  Husky
  VERSION 0.0.1
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG main
)

FetchContent_MakeAvailable(sdl3)

FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG master
)

FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)

set(
  RENDER_SOURCES
  "render/allocator.cpp"
  "render/allocator.h"
  "render/canvas.h"
  "render/command_builder.h"
  "render/command_builder.cpp"
  "render/engine.cpp"
  "render/engine.h"
  "render/frames.cpp"
  "render/frames.h"
  "render/image.cpp"
  "render/image.h"
  "render/swapchain.cpp"
  "render/swapchain.h"
  "render/vk_base.h"
  "render/vk_base.cpp"
  "render/window.h"
  "render/window.cpp"
)

add_executable(${PROJECT_NAME} 
  main.cpp
  ${RENDER_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
  SDL3::SDL3
  Vulkan::Headers
  GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PRIVATE 
  ${SDL3_SOURCE_DIR}/include
  ${VMA_SOURCE_DIR}/include
)
