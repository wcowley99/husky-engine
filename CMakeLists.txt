cmake_minimum_required(VERSION 3.20)
project(Husky)

include(FetchContent)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG main
)

FetchContent_MakeAvailable(sdl3)

add_executable(husky-engine main.cpp)
target_link_libraries(husky-engine PRIVATE SDL3::SDL3)

FetchContent_GetProperties(SDL3)
if(SDL3_POPULATED)
    target_include_directories(MyExecutable PRIVATE ${SDL3_SOURCE_DIR}/include)
endif()
