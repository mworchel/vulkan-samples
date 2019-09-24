#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <string>
#include <vector>

int main()
{
    // Vulkan test
    auto instanceExtensions = vk::enumerateInstanceExtensionProperties();

    std::cout << "Vulkan default instance extensions:" << std::endl;
    for (auto extension : instanceExtensions)
    {
        std::cout << extension.extensionName << std::endl;
    }

    // GLM test
    glm::mat4 matrix;
    glm::vec4 vec;
    auto test = matrix * vec;

    // GLFW test
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}