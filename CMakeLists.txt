cmake_minimum_required(VERSION 3.20)

project(
  Husky
  VERSION 0.0.1
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Vulkan REQUIRED)
find_package(Python)

add_subdirectory(external)
add_subdirectory(src)
