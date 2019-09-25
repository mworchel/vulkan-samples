cmake_minimum_required(VERSION 3.10)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/utils.cmake")

project(vulkan-samples)

find_package(Vulkan REQUIRED)

find_package(glm REQUIRED)

find_package(glfw3 REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/00-basic-setup)
add_subdirectory(${CMAKE_SOURCE_DIR}/01-drawing-triangle)