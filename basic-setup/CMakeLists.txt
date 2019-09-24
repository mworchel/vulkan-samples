add_executable(basic-setup main.cpp)

target_include_directories(basic-setup PRIVATE ${GLM_INCLUDE_DIRS})
target_link_libraries(basic-setup Vulkan::Vulkan glfw)