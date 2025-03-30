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

find_package(Vulkan REQUIRED)

set(
  RENDER_SOURCES
  "render/canvas.h"
  "render/command_builder.h"
  "render/command_builder.cpp"
  "render/engine.cpp"
  "render/engine.h"
  "render/frames.h"
  "render/frames.cpp"
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
)

FetchContent_GetProperties(SDL3)
if(SDL3_POPULATED)
    target_include_directories(MyExecutable PRIVATE ${SDL3_SOURCE_DIR}/include)
endif()
