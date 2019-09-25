#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <set>

constexpr int WINDOW_WIDTH  = 800;
constexpr int WINDOW_HEIGHT = 600;

template<typename Type>
class MyOptional
{
public:
    MyOptional()
        : m_val()
        , m_valid(false)
    {
    }

    MyOptional(Type&& val)
        : m_val(std::forward<Type>(val))
        , m_valid(true)
    {
    }

    Type get()
    {
        return m_val;
    }

    bool hasValue()
    {
        return m_valid;
    }

    template<typename Type>
    MyOptional& operator=(MyOptional<Type> const& other)
    {
        if (other.m_valid)
        {
            m_val   = other.m_val;
            m_valid = other.m_valid;
        }
        else
        {
            m_val   = Type();
            m_valid = false;
        }

        return *this;
    }

    template<typename Type>
    MyOptional& operator=(Type const& val)
    {
        m_val   = val;
        m_valid = true;

        return *this;
    }

private:
    Type m_val;
    bool m_valid;
};

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
    MyOptional<uint32_t> graphicsFamily;
    MyOptional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.hasValue() &&
               presentFamily.hasValue();
    }
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
        applicationInfo.apiVersion         = VK_API_VERSION_1_1;

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
                       [](vk::ExtensionProperties const& p) { return p.extensionName; });

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
                       [](vk::LayerProperties const& p) { return p.layerName; });

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
        m_debugMessenger                                = m_instance.createDebugUtilsMessengerEXT(createInfo, nullptr, vk::DispatchLoaderDynamic(m_instance));
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

    bool isDeviceSuitable(vk::PhysicalDevice const& device)
    {
        auto queueFamilies = findQueueFamilies(device);

        return queueFamilies.isComplete();
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

        std::set<uint32_t> uniqueQueueFamilies{ indices.graphicsFamily.get(), indices.presentFamily.get() };

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            vk::DeviceQueueCreateInfo queueCreateInfo;
            queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.get();
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

        createInfo.enabledExtensionCount = 0U;

        auto requiredLayers = getRequiredLayers();

        createInfo.ppEnabledLayerNames = requiredLayers.data();
        createInfo.enabledLayerCount   = static_cast<uint32_t>(requiredLayers.size());

        m_device = m_physicalDevice.createDevice(createInfo);

        m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.get(), 0U);
        m_presentQueue = m_device.getQueue(indices.presentFamily.get(), 0U);
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

        m_device.destroy();

#if !defined(NDEBUG)
        m_instance.destroyDebugUtilsMessengerEXT(m_debugMessenger, nullptr, vk::DispatchLoaderDynamic(m_instance));
#endif
        m_instance.destroySurfaceKHR(m_surface);
        m_instance.destroy();
    }

    GLFWwindow*  m_window;
    vk::Instance m_instance;
#if !defined(NDEBUG)
    vk::DebugUtilsMessengerEXT m_debugMessenger;
#endif
    vk::SurfaceKHR     m_surface;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device         m_device;
    vk::Queue          m_graphicsQueue;
    vk::Queue          m_presentQueue;
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