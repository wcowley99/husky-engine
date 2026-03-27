project(
  external
  VERSION 0.0.1
  LANGUAGES C CXX
)

add_subdirectory(volk)
add_subdirectory(cglm)
add_subdirectory(assimp)

add_library(external 
  VulkanMemoryAllocator/src/VmaUsage.cpp
)

target_include_directories(external PRIVATE
  VulkanMemoryAllocator/include
)

target_link_libraries(external PRIVATE 
  Vulkan::Headers
  assimp
)

target_compile_definitions(external PUBLIC 
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYANMIC_VULKAN_FUNCTIONS=0
)
