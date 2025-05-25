cmake_minimum_required(VERSION 3.20)

project(
  civ-game
  VERSION 0.0.1
  LANGUAGES C CXX
)


# Generate Shader Headers
file(GLOB SHADERS CONFIGURE_DEPENDS "shaders/*")

add_executable(hexembed scripts/hexembed.c)

set(SHADER_HEADERS "")
foreach(SHADER ${SHADERS})
  message(${SHADER})
  get_filename_component(NAME ${SHADER} NAME_WE)
  get_filename_component(EXT ${SHADER} EXT)
  string(REPLACE "." "" EXT "${EXT}")

  set(SHADER_NAME "${NAME}_${EXT}")
  set(SPIRV_FILE "${CMAKE_BINARY_DIR}/${SHADER_NAME}.spv")
  set(OUTPUT_HEADER "${CMAKE_BINARY_DIR}/${SHADER_NAME}.h")
  list(APPEND SHADER_HEADERS ${OUTPUT_HEADER})

  add_custom_command(
    OUTPUT ${OUTPUT_HEADER}
    BYPRODUCTS ${SPIRV_FILE}
    COMMAND glslc ${SHADER} -o ${SPIRV_FILE} && ${CMAKE_BINARY_DIR}/Debug/hexembed.exe ${SPIRV_FILE} ${SHADER_NAME} > ${OUTPUT_HEADER}
    DEPENDS hexembed
    COMMENT "Generating ${OUTPUT_HEADER} from ${SHADER}"
    VERBATIM
  )
endforeach()

add_custom_target(generated_headers ALL
  DEPENDS ${SHADER_HEADERS}
)

# Build Source
find_package(Vulkan REQUIRED)

add_subdirectory(external/cglm EXCLUDE_FROM_ALL)
add_subdirectory(external/SDL)
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

add_executable(civ-game
  src/main.c
  src/renderer.c
  src/renderer.h
  ${SHADER_HEADERS}
)

add_dependencies(civ-game generated_headers)

target_link_libraries(civ-game PRIVATE
  SDL3::SDL3
  Vulkan::Headers
  VulkanMemoryAllocator
  cglm_headers
  volk
)

target_include_directories(civ-game PRIVATE
  external/SDL/include
  external/VulkanMemoryAllocator/include
  external/cglm/include
  external/volk
  ${CMAKE_BINARY_DIR}
)