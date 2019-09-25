set(SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/triangle-shader.vert")

add_executable(drawing-triangle main.cpp ${SHADER_FILES})

target_include_directories(drawing-triangle PRIVATE ${GLM_INCLUDE_DIRS})
target_link_libraries(drawing-triangle Vulkan::Vulkan glfw)

add_shader_compile_target(drawing-triangle ${SHADER_FILES})