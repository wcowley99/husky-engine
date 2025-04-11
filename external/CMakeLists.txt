cmake_minimum_required(VERSION 3.20)

add_subdirectory(SDL)
add_subdirectory(VulkanMemoryAllocator)

find_package(Vulkan REQUIRED)

message("found vulkan? ${Vulkan_FOUND}")


set(IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui")
set(IMGUI_SOURCE_DIR ${IMGUI_SOURCE_DIR} PARENT_SCOPE)

set(IMGUI_SOURCES 
  "${IMGUI_SOURCE_DIR}/imgui.cpp"
  "${IMGUI_SOURCE_DIR}/imgui.h"
  "${IMGUI_SOURCE_DIR}/imgui_demo.cpp"
  "${IMGUI_SOURCE_DIR}/imgui_draw.cpp"
  "${IMGUI_SOURCE_DIR}/imgui_internal.h"
  "${IMGUI_SOURCE_DIR}/imgui_tables.cpp"
  "${IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
  "${IMGUI_SOURCE_DIR}/imstb_rectpack.h"
  "${IMGUI_SOURCE_DIR}/imstb_textedit.h"
  "${IMGUI_SOURCE_DIR}/imstb_truetype.h"
  "${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.h"
  "${IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
  "${IMGUI_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
  PARENT_SCOPE
)

