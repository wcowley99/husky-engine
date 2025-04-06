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

FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG master
)

FetchContent_MakeAvailable(sdl3)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)
find_package(Python)

message("Vulkan: ${Vulkan_FOUND}")
message("glslc: ${Vulkan_glslc_FOUND}")
message("glslangValidator: ${Vulkan_glslangValidator_FOUND}")
message("glslang ${Vulkan_glslang_FOUND}")
message("shaderc_combined: ${Vulkan_shaderc_combined_FOUND}")
message("SPIRV-Tools: ${Vulkan_SPIRV-Tools_FOUND}")

set(
  RENDER_SOURCES
  "render/allocator.cpp"
  "render/allocator.h"
  "render/canvas.h"
  "render/command_builder.h"
  "render/command_builder.cpp"
  "render/descriptors.cpp"
  "render/descriptors.h"
  "render/engine.cpp"
  "render/engine.h"
  "render/frames.cpp"
  "render/frames.h"
  "render/image.cpp"
  "render/image.h"
  "render/pipelines.cpp"
  "render/pipelines.h"
  "render/swapchain.cpp"
  "render/swapchain.h"
  "render/vk_base.h"
  "render/vk_base.cpp"
  "render/window.h"
  "render/window.cpp"
)

add_custom_command(
  OUTPUT shader_sources.h
  COMMAND python 
  ARGS ${CMAKE_SOURCE_DIR}/embed_shaders.py ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}
  DEPENDS embed_shaders.py
)

set(
  GENERATED_FILES
  "${CMAKE_CURRENT_BINARY_DIR}/shader_sources.h"
)

add_executable(${PROJECT_NAME} 
  main.cpp
  ${RENDER_SOURCES}
  ${GENERATED_FILES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
  SDL3::SDL3
  Vulkan::Headers
  GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PRIVATE 
  ${SDL3_SOURCE_DIR}/include
  ${VMA_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
)
