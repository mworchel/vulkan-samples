add_executable(drawing-triangle main.cpp)

target_include_directories(drawing-triangle PRIVATE ${GLM_INCLUDE_DIRS})
target_link_libraries(drawing-triangle Vulkan::Vulkan glfw)