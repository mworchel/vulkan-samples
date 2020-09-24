# Vulkan Samples

Growing repository of Vulkan samples implementing the [Vulkan tutorials](https://vulkan-tutorial.com/) and more (soon™). After having worked a lot with Direct3D 11 in the past, this is the documentation of my journey to Vulkan.

Highlights of the implementation are
- **CMake** as meta build system
- **HLSL** shaders (memories 🙌)
    - Compiled to SPIR-V
    - Cross-compiled to GLSL for validation
- **C++17** using **Vulkan-Hpp**

## Requirements and Dependencies

- CMake >= 3.10
- Vulkan SDK >= 1.2.148.1 
- GLM
- GLFW >= 3