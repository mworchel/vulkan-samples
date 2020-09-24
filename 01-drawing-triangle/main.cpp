#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>

constexpr int WINDOW_WIDTH         = 800;
constexpr int WINDOW_HEIGHT        = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

static std::vector<char> readFile(std::string const& filename)
{
    std::ifstream file(filename, std::ios_base::ate | std::ios_base::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file '" + filename + "'");
    }
    
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    return buffer;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
              VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
              void*                                       pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        // Important message
    }

    return VK_FALSE;
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() &&
               presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR        capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR>   presentModes;
};

class HelloTriangleApplication
{
public:
    void run()
    {
        initialize();
        mainLoop();
        uninitialize();
    }

private:
    void initialize()
    {
        initializeWindow();
        initializeVulkan();
    }

    void initializeWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan", nullptr, nullptr);
    }

    void initializeVulkan()
    {
        createInstance();
#if !defined(NDEBUG)
        createDebugMessenger();
#endif
        createSurface();
        selectPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance()
    {
        auto requiredLayers = getRequiredLayers();

        auto missingLayers = checkLayerSupport(requiredLayers);
        if (!missingLayers.empty())
        {
            // TODO: Output missing layers
            throw std::runtime_error("layers requested, but not available");
        }

        vk::ApplicationInfo applicationInfo;
        applicationInfo.pApplicationName   = "Drawing Triangle";
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.apiVersion         = VK_API_VERSION_1_2;

        vk::InstanceCreateInfo instanceCreateInfo;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
        instanceCreateInfo.enabledLayerCount   = static_cast<uint32_t>(requiredLayers.size());

        auto requiredExtensions = getRequiredExtensions();

        // Check if all required extensions are present
        auto                     availableExtensions = vk::enumerateInstanceExtensionProperties();
        std::vector<std::string> availableExtensionNames(availableExtensions.size());
        std::transform(std::begin(availableExtensions), std::end(availableExtensions),
                       std::begin(availableExtensionNames),
                       [](vk::ExtensionProperties const& p) { return std::string(p.extensionName.data()); });

        for (char const* requiredExtension : requiredExtensions)
        {
            if (std::end(availableExtensionNames) == std::find(std::begin(availableExtensionNames), std::end(availableExtensionNames), std::string(requiredExtension)))
            {
                std::cerr << "required extension '" << requiredExtension << "' not present." << std::endl;
            }
        }

        instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
        instanceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());

#if !defined(NDEBUG)
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = getDebugMessengerCreateInfo();
        instanceCreateInfo.pNext                               = &messengerCreateInfo;
#endif

        m_instance = vk::createInstance(instanceCreateInfo);
    }

    std::vector<char const*> getRequiredLayers()
    {
        std::vector<const char*> layers;

#if !defined(NDEBUG)
        layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

        return layers;
    }

    std::vector<std::string> checkLayerSupport(std::vector<const char*> const& requiredLayers)
    {
        auto instanceLayerProperties = vk::enumerateInstanceLayerProperties();

        std::vector<std::string> instanceLayers(instanceLayerProperties.size());
        std::transform(std::begin(instanceLayerProperties), std::end(instanceLayerProperties),
                       std::begin(instanceLayers),
                       [](vk::LayerProperties const& p) { return std::string(p.layerName.data()); });

        std::vector<std::string> missingLayers;
        for (auto const& layerName : requiredLayers)
        {
            if (std::find(std::begin(instanceLayers), std::end(instanceLayers), std::string(layerName)) == std::end(instanceLayers))
            {
                missingLayers.push_back(layerName);
            }
        }

        return missingLayers;
    }

    std::vector<char const*> getRequiredExtensions()
    {
        uint32_t     glfwNumExtension = 0U;
        char const** glfwExtensions   = glfwGetRequiredInstanceExtensions(&glfwNumExtension);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwNumExtension);

#if !defined(NDEBUG)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        return extensions;
    }

    std::vector<char const*> getRequiredDeviceExtensions()
    {
        return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    }

    bool checkDeviceExtensionSupport(vk::PhysicalDevice const& device, std::vector<char const*> const& requiredExtensionNames)
    {
        auto deviceExtensions = device.enumerateDeviceExtensionProperties();

        std::vector<std::string> deviceExtensionNames(deviceExtensions.size());
        std::transform(std::begin(deviceExtensions), std::end(deviceExtensions),
                       std::begin(deviceExtensionNames),
                       [](vk::ExtensionProperties const& e) { return std::string(e.extensionName.data()); });

        return std::all_of(std::begin(requiredExtensionNames), std::end(requiredExtensionNames),
                           [&deviceExtensionNames](char const* name) { return std::find(std::begin(deviceExtensionNames), std::end(deviceExtensionNames), std::string(name)) != std::end(deviceExtensionNames); });
    }

    vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo;
        createInfo.messageSeverity =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        createInfo.messageType =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        createInfo.pfnUserCallback = debugCallback;

        return createInfo;
    }

    void createDebugMessenger()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo = getDebugMessengerCreateInfo();        
        m_debugMessenger                                = m_instance.createDebugUtilsMessengerEXT(createInfo, nullptr, vk::DispatchLoaderDynamic(m_instance, &vkGetInstanceProcAddr));
    }

    void createSurface()
    {
        VkSurfaceKHR surfaceRaw;
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &surfaceRaw) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface");
        }

        m_surface = surfaceRaw;
    }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice const& device)
    {
        QueueFamilyIndices indices;

        auto familyProperties = device.getQueueFamilyProperties();

        for (uint32_t i = 0; i < familyProperties.size(); ++i)
        {
            auto const& queueFamily = familyProperties[i];

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphicsFamily = i;
            }

            bool hasPresentSupport = device.getSurfaceSupportKHR(i, m_surface);

            if (queueFamily.queueCount > 0 && hasPresentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }
        }

        return indices;
    }

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice const& device)
    {
        SwapChainSupportDetails details;

        details.capabilities = device.getSurfaceCapabilitiesKHR(m_surface);
        details.formats      = device.getSurfaceFormatsKHR(m_surface);
        details.presentModes = device.getSurfacePresentModesKHR(m_surface);

        return details;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for (vk::SurfaceFormatKHR const& format : availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return format;
            }
        }

        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for (const auto& presentMode : availablePresentModes)
        {
            if (presentMode == vk::PresentModeKHR::eMailbox)
            {
                return presentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            return capabilities.currentExtent;
        }
        else
        {
            vk::Extent2D actualExtent(WINDOW_WIDTH, WINDOW_HEIGHT);

            actualExtent.width  = std::min(capabilities.maxImageExtent.width, std::max(capabilities.minImageExtent.width, actualExtent.width));
            actualExtent.height = std::min(capabilities.maxImageExtent.height, std::max(capabilities.minImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    bool isDeviceSuitable(vk::PhysicalDevice const& device)
    {
        auto queueFamilies = findQueueFamilies(device);

        bool requiredExtensionsSupported = checkDeviceExtensionSupport(device, getRequiredDeviceExtensions());

        bool swapChainAdequate = false;
        if (requiredExtensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return queueFamilies.isComplete() && requiredExtensionsSupported && swapChainAdequate;
    }

    void selectPhysicalDevice()
    {
        auto physicalDevices = m_instance.enumeratePhysicalDevices();

        if (physicalDevices.empty())
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        auto it = std::find_if(std::begin(physicalDevices), std::end(physicalDevices), [this](auto&& d) { return isDeviceSuitable(d); });
        if (it == std::end(physicalDevices))
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        else
        {
            m_physicalDevice = *it;
        }
    }

    void createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        std::set<uint32_t> uniqueQueueFamilies{indices.graphicsFamily.value(), indices.presentFamily.value()};

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            vk::DeviceQueueCreateInfo queueCreateInfo;
            queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
            queueCreateInfo.queueCount       = 1U;
            float priority                   = 1.0f;
            queueCreateInfo.pQueuePriorities = &priority;

            queueCreateInfos.push_back(queueCreateInfo);
        }

        vk::PhysicalDeviceFeatures deviceFeatures;

        vk::DeviceCreateInfo createInfo;
        createInfo.pEnabledFeatures     = &deviceFeatures;
        createInfo.pQueueCreateInfos    = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

        auto requiredDeviceExtensions      = getRequiredDeviceExtensions();
        createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtensions.size());

        auto requiredLayers = getRequiredLayers();

        createInfo.ppEnabledLayerNames = requiredLayers.data();
        createInfo.enabledLayerCount   = static_cast<uint32_t>(requiredLayers.size());

        m_device = m_physicalDevice.createDevice(createInfo);

        m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0U);
        m_presentQueue  = m_device.getQueue(indices.presentFamily.value(), 0U);
    }

    void createSwapChain()
    {
        auto swapChainSupport = querySwapChainSupport(m_physicalDevice);

        m_swapchainExtent      = chooseSwapExtent(swapChainSupport.capabilities);
        auto format            = chooseSwapSurfaceFormat(swapChainSupport.formats);
        m_swapchainImageFormat = format.format;
        auto mode              = chooseSwapPresentMode(swapChainSupport.presentModes);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.surface          = m_surface;
        createInfo.minImageCount    = imageCount;
        createInfo.imageFormat      = format.format;
        createInfo.imageColorSpace  = format.colorSpace;
        createInfo.imageExtent      = m_swapchainExtent;
        createInfo.imageArrayLayers = 1U;
        createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;

        QueueFamilyIndices    indices = findQueueFamilies(m_physicalDevice);
        std::vector<uint32_t> queueFamilyIndices{indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily.value() != indices.presentFamily.value())
        {
            createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
        }
        else
        {
            createInfo.imageSharingMode      = vk::SharingMode::eExclusive;
            createInfo.queueFamilyIndexCount = 0;       // Optional
            createInfo.pQueueFamilyIndices   = nullptr; // Optional
        }

        createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

        createInfo.presentMode = mode;
        createInfo.clipped     = VK_TRUE;

        createInfo.oldSwapchain = nullptr;

        m_swapchain = m_device.createSwapchainKHR(createInfo);
        m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapchain);
    }

    void createImageViews()
    {
        for (auto const& image : m_swapchainImages)
        {
            vk::ImageViewCreateInfo createInfo;
            createInfo.image    = image;
            createInfo.format   = m_swapchainImageFormat;
            createInfo.viewType = vk::ImageViewType::e2D;

            createInfo.components.r = vk::ComponentSwizzle::eIdentity;
            createInfo.components.g = vk::ComponentSwizzle::eIdentity;
            createInfo.components.b = vk::ComponentSwizzle::eIdentity;
            createInfo.components.a = vk::ComponentSwizzle::eIdentity;

            createInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel   = 0U;
            createInfo.subresourceRange.levelCount     = 1U;
            createInfo.subresourceRange.baseArrayLayer = 0U;
            createInfo.subresourceRange.layerCount     = 1U;

            m_swapchainImageViews.push_back(m_device.createImageView(createInfo));
        }
    }

    vk::ShaderModule createShaderModule(const std::vector<char>& code)
    {
        vk::ShaderModuleCreateInfo createInfo;
        createInfo.codeSize = code.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

        return m_device.createShaderModule(createInfo);
    }

    void createRenderPass()
    {
        vk::AttachmentDescription colorAttachment;
        colorAttachment.format  = m_swapchainImageFormat;
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        // These operations apply to color and depth data
        colorAttachment.loadOp  = vk::AttachmentLoadOp::eClear;  // What to do with the data in the attachment before rendering...
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore; // ...and what to do after rendering.
        // These operations apply to stencil data
        colorAttachment.stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout  = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout    = vk::ImageLayout::ePresentSrcKHR;
        
        // "The layout specifies which layout we would like the attachment to have during a subpass that uses this reference"
        vk::AttachmentReference colorAttachmentRef;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = vk::ImageLayout::eColorAttachmentOptimal;

        // "Vulkan may also support compute subpasses in the future, so we have to be explicit about this being a graphics subpass"
        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint    = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;

        vk::SubpassDependency dependency;
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.srcAccessMask = vk::AccessFlags();
        dependency.dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        m_renderPass = m_device.createRenderPass(renderPassInfo);
    }

    void createGraphicsPipeline()
    {
        auto vertShaderCode = readFile(PATH_TRIANGLE_SHADER_VERT);
        auto fragShaderCode = readFile(PATH_TRIANGLE_SHADER_FRAG);

        auto vertShaderModule = createShaderModule(vertShaderCode);
        auto fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName  = "main";
        // Can be used for specifying/defining constants in the shader code:
        // vertShaderStageInfo.pSpecializationInfo 

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName  = "main";

        vk::PipelineShaderStageCreateInfo shaderStages[] = { 
            vertShaderStageInfo, fragShaderStageInfo
        };

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
        vertexInputInfo.pVertexBindingDescriptions      = nullptr;
        vertexInputInfo.vertexBindingDescriptionCount   = 0U;
        vertexInputInfo.pVertexAttributeDescriptions    = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0U;

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        vk::Viewport viewport;
        viewport.x        = 0.f;
        viewport.y        = 0.f;
        viewport.width    = static_cast<float>(m_swapchainExtent.width);
        viewport.height   = static_cast<float>(m_swapchainExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        vk::Rect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent = m_swapchainExtent;

        vk::PipelineViewportStateCreateInfo viewportState;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer;
        // "Using this requires enabling a GPU feature."
        rasterizer.depthClampEnable = VK_FALSE;
        // With this flag, geometry never passes through the rasterizer, so there is no output
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        // "Using any mode other than fill requires enabling a GPU feature."
        rasterizer.polygonMode             = vk::PolygonMode::eFill;
        // "any line thicker than 1.0f requires you to enable the wideLines GPU feature"
        rasterizer.lineWidth               = 1.f;
        rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace               = vk::FrontFace::eClockwise;
        // "The rasterizer can alter the depth values by adding a constant value 
        // or biasing them based on a fragment's slope. 
        // This is sometimes used for shadow mapping, but we won't be using it." 
        rasterizer.depthBiasEnable         = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp          = 0.0f;
        rasterizer.depthBiasSlopeFactor    = 0.0f;

        // "Enabling it requires enabling a GPU feature."
        vk::PipelineMultisampleStateCreateInfo multisampling;
        multisampling.sampleShadingEnable   = VK_FALSE;
        multisampling.rasterizationSamples  = vk::SampleCountFlagBits::e1;
        multisampling.minSampleShading      = 1.0f;     // Optional
        multisampling.pSampleMask           = nullptr;  // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable      = VK_FALSE; // Optional

        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        colorBlendAttachment.colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable         = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne;  // Optional
        colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
        colorBlendAttachment.colorBlendOp        = vk::BlendOp::eAdd;      // Optional
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;  // Optional
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
        colorBlendAttachment.alphaBlendOp        = vk::BlendOp::eAdd;      // Optional

        vk::PipelineColorBlendStateCreateInfo colorBlending;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.logicOp           = vk::LogicOp::eCopy;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        vk::DynamicState dynamicStates[] = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eLineWidth
        };

        vk::PipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates    = dynamicStates;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        pipelineLayoutInfo.setLayoutCount         = 0;
        pipelineLayoutInfo.pSetLayouts            = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0;       // Optional
        pipelineLayoutInfo.pPushConstantRanges    = nullptr; // Optional

        m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutInfo);

        vk::GraphicsPipelineCreateInfo pipelineInfo;
        // Dynamic parts
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages    = shaderStages;
        // Fixed function parts
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = nullptr;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = nullptr;
        pipelineInfo.layout              = m_pipelineLayout;
        pipelineInfo.renderPass          = m_renderPass;
        pipelineInfo.subpass             = 0;
        // "Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline."
        // "These values are only used if the VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field."
        pipelineInfo.basePipelineHandle  = vk::Pipeline(); // Optional
        pipelineInfo.basePipelineIndex   = -1;             // Optional

        m_graphicsPipeline = m_device.createGraphicsPipelines(vk::PipelineCache(), { pipelineInfo }).value[0];

        m_device.destroyShaderModule(vertShaderModule);
        m_device.destroyShaderModule(fragShaderModule);
    }

    void createFramebuffers()
    {
        m_swapchainFramebuffers.resize(m_swapchainImageViews.size());

        for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
        {
            vk::ImageView attachments[] = {m_swapchainImageViews[i]};

            // A framebuffer object wraps all the attachments.
            // Therefore, it has to be bound to a render pass that is compatible (expects compatible attachments).
            vk::FramebufferCreateInfo framebufferInfo;
            framebufferInfo.renderPass      = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments    = attachments;
            framebufferInfo.width           = m_swapchainExtent.width;
            framebufferInfo.height          = m_swapchainExtent.height;
            framebufferInfo.layers          = 1;

            m_swapchainFramebuffers[i] = m_device.createFramebuffer(framebufferInfo);
        }
    }

    void createCommandPool()
    {
        auto queueFamilyIndices = findQueueFamilies(m_physicalDevice);

        vk::CommandPoolCreateInfo poolInfo;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        poolInfo.flags            = vk::CommandPoolCreateFlags();               // Optional

        m_commandPool = m_device.createCommandPool(poolInfo);
    }

    void createCommandBuffers()
    {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool        = m_commandPool;
        // Primary command buffers can be submitted to a queue
        // but not executed from other command buffers.
        // For secondary ones it's the other way around.
        allocInfo.level              = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = m_swapchainFramebuffers.size();

        m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);

        for (size_t i = 0; i < m_commandBuffers.size(); i++)
        {
            vk::CommandBufferBeginInfo beginInfo;
            beginInfo.flags            = vk::CommandBufferUsageFlags();
            beginInfo.pInheritanceInfo = nullptr;

            // "If the command buffer was already recorded once, 
            // then a call to vkBeginCommandBuffer will implicitly reset it".
            m_commandBuffers[i].begin(beginInfo);

            vk::RenderPassBeginInfo renderPassInfo;
            renderPassInfo.renderPass        = m_renderPass;
            renderPassInfo.framebuffer       = m_swapchainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchainExtent; 

            vk::ClearValue clearColor      = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues    = &clearColor;

            m_commandBuffers[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
            m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);
            m_commandBuffers[i].draw(3, 1, 0, 0);
            m_commandBuffers[i].endRenderPass();

            m_commandBuffers[i].end();
        }
    }

    void createSyncObjects()
    {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        m_imagesInFlight.resize(m_swapchainImages.size());

        vk::SemaphoreCreateInfo semaphoreInfo;
        vk::FenceCreateInfo     fenceInfo;
        fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_imageAvailableSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
            m_renderFinishedSemaphores[i] = m_device.createSemaphore(semaphoreInfo);   
            m_inFlightFences[i]           = m_device.createFence(fenceInfo);
        }

    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }

        m_device.waitIdle();
    }

    void drawFrame()
    {
        m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], vk::Fence());

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if (m_imagesInFlight[imageIndex])
        {
            m_device.waitForFences(1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }

        // Mark the image as now being in use by this frame
        m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

        vk::SubmitInfo submitInfo;

        // "Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores."
        vk::Semaphore          waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
        vk::PipelineStageFlags waitStages[]     = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        submitInfo.waitSemaphoreCount           = 1;
        submitInfo.pWaitSemaphores              = waitSemaphores;
        submitInfo.pWaitDstStageMask            = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &m_commandBuffers[imageIndex];

        vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
        submitInfo.signalSemaphoreCount  = 1;
        submitInfo.pSignalSemaphores     = signalSemaphores;

        m_device.resetFences(1, &m_inFlightFences[m_currentFrame]);
        m_graphicsQueue.submit(1, &submitInfo, m_inFlightFences[m_currentFrame]);

        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSemaphores;

        vk::SwapchainKHR swapchains[] = {m_swapchain};
        presentInfo.swapchainCount    = 1;
        presentInfo.pSwapchains       = swapchains;
        presentInfo.pImageIndices     = &imageIndex;
        
        presentInfo.pResults = nullptr;

        m_presentQueue.presentKHR(presentInfo);

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void uninitialize()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_device.destroySemaphore(m_imageAvailableSemaphores[i]);
            m_device.destroySemaphore(m_renderFinishedSemaphores[i]);
            m_device.destroyFence(m_inFlightFences[i]);
        }

        m_device.destroyCommandPool(m_commandPool);

        for (auto framebuffer : m_swapchainFramebuffers)
        {
            m_device.destroyFramebuffer(framebuffer);
        }

        for (auto imageView : m_swapchainImageViews)
        {
            m_device.destroyImageView(imageView);
        }

        m_device.destroyPipeline(m_graphicsPipeline);
        m_device.destroyPipelineLayout(m_pipelineLayout);
        m_device.destroyRenderPass(m_renderPass);
        m_device.destroySwapchainKHR(m_swapchain);
        m_device.destroy();

#if !defined(NDEBUG)
        m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr, vk::DispatchLoaderDynamic(m_instance, &vkGetInstanceProcAddr));
#endif
        m_instance.destroySurfaceKHR(m_surface);
        m_instance.destroy();
    }

    GLFWwindow*  m_window;
    vk::Instance m_instance;
#if !defined(NDEBUG)
    vk::DebugUtilsMessengerEXT m_debugMessenger;
#endif
    vk::SurfaceKHR                 m_surface;
    vk::PhysicalDevice             m_physicalDevice;
    vk::Device                     m_device;
    vk::Queue                      m_graphicsQueue;
    vk::Queue                      m_presentQueue;
    vk::SwapchainKHR               m_swapchain;
    vk::Format                     m_swapchainImageFormat;
    vk::Extent2D                   m_swapchainExtent;
    std::vector<vk::Image>         m_swapchainImages;
    std::vector<vk::ImageView>     m_swapchainImageViews;
    vk::RenderPass                 m_renderPass;
    vk::PipelineLayout             m_pipelineLayout;
    vk::Pipeline                   m_graphicsPipeline;
    std::vector<vk::Framebuffer>   m_swapchainFramebuffers;
    vk::CommandPool                m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<vk::Semaphore>     m_imageAvailableSemaphores;
    std::vector<vk::Semaphore>     m_renderFinishedSemaphores;
    std::vector<vk::Fence>         m_inFlightFences;
    std::vector<vk::Fence>         m_imagesInFlight;
    size_t                         m_currentFrame = 0;
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}