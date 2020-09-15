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

constexpr int WINDOW_WIDTH  = 800;
constexpr int WINDOW_HEIGHT = 600;

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
        createSwapChainImageViews();
    }

    void createInstance()
    {
        auto requiredLayers = getRequiredLayers();

        if (!checkLayerSupport(requiredLayers))
        {
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

    bool checkLayerSupport(std::vector<const char*> const& requiredLayers)
    {
        auto instanceLayerProperties = vk::enumerateInstanceLayerProperties();

        std::vector<std::string> instanceLayers(instanceLayerProperties.size());
        std::transform(std::begin(instanceLayerProperties), std::end(instanceLayerProperties),
                       std::begin(instanceLayers),
                       [](vk::LayerProperties const& p) { return std::string(p.layerName.data()); });

        return std::all_of(std::begin(requiredLayers), std::end(requiredLayers),
                           [&instanceLayers](char const* requiredLayer) {
                               return std::find(std::begin(instanceLayers), std::end(instanceLayers), std::string(requiredLayer)) != std::end(instanceLayers);
                           });
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
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
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

    void createSwapChainImageViews()
    {
        for (auto const& image : m_swapchainImages)
        {
            vk::ImageViewCreateInfo createInfo;
            createInfo.image    = image;
            createInfo.format   = m_swapchainImageFormat;
            createInfo.viewType = vk::ImageViewType::e2D;

            createInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
            createInfo.subresourceRange.baseMipLevel   = 0U;
            createInfo.subresourceRange.levelCount     = 1U;
            createInfo.subresourceRange.baseArrayLayer = 0U;
            createInfo.subresourceRange.layerCount     = 1U;

            m_swapchainImageViews.push_back(m_device.createImageView(createInfo));
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
        }
    }

    void uninitialize()
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();

        for (auto imageView : m_swapchainImageViews)
        {
            m_device.destroyImageView(imageView);
        }

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
    vk::SurfaceKHR             m_surface;
    vk::PhysicalDevice         m_physicalDevice;
    vk::Device                 m_device;
    vk::Queue                  m_graphicsQueue;
    vk::Queue                  m_presentQueue;
    vk::SwapchainKHR           m_swapchain;
    vk::Format                 m_swapchainImageFormat;
    vk::Extent2D               m_swapchainExtent;
    std::vector<vk::Image>     m_swapchainImages;
    std::vector<vk::ImageView> m_swapchainImageViews;
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