#include "RendererContext.h"

#include "Vulkan.h"

#pragma warning(disable : 4005)

#include <glfw/glfw3.h>

#include <vector>
#include <Windows.h> // MessageBox

#pragma warning(default : 4005)

#ifndef VK_API_VERSION_1_3
    #error Wrong vulkan SDK!
#endif

namespace vkPlayground::Renderer {

#ifdef PLAYGROUND_DEBUG
    bool s_Validation = true;
#else
    bool s_Validation = false;
#endif

    namespace Utils {

        static bool CheckDriverAPIVersionSupport(uint32_t minimumSupportedVersion)
        {
            uint32_t instanceVersion = 0;
            vkEnumerateInstanceVersion(&instanceVersion);

            if (instanceVersion < minimumSupportedVersion)
            {
                std::cerr << "[Renderer] Incompatible driver version!\n";
                std::cerr << "[Renderer] \tYou have: " << VK_API_VERSION_MAJOR(instanceVersion) << "." << VK_API_VERSION_MINOR(instanceVersion) << "." << VK_API_VERSION_PATCH(instanceVersion) << std::endl;
                std::cerr << "[Renderer] \tYou need at least: " << VK_API_VERSION_MAJOR(minimumSupportedVersion) << "." << VK_API_VERSION_MINOR(minimumSupportedVersion) << "." << VK_API_VERSION_PATCH(minimumSupportedVersion) << std::endl;

                return false;
            }

            return true;
        }

#if PG_USE_DEBUG_REPORT_CALLBACK_EXT
        static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
            VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objectType,
            uint64_t object,
            size_t location,
            int32_t messageCode,
            const char* pLayerPrefix,
            const char* pMessage,
            void* pUserData)
        {
            (void)flags, (void)object, (void)location, (void)messageCode, (void)pUserData, (void)pLayerPrefix;
            std::cout << "[Renderer] VulkanDebugReportCallback:\n" << "\tObject Type: " << objectType << "\n\tMessage: " << pMessage << std::endl;

            return VK_FALSE;
        }
#endif

        static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void*)
        {
            std::cerr << "[ValidationLayers] " << pCallbackData->pMessage << std::endl;

            return VK_FALSE;
        }

    }

    VkInstance RendererContext::s_VulkanInstance = nullptr;

    Scope<RendererContext> RendererContext::Create()
    {
        return CreateScope<RendererContext>();
    }

    RendererContext::RendererContext()
    {
    }

    RendererContext::~RendererContext()
    {
        if (s_Validation)
        {
#if PG_USE_DEBUG_REPORT_CALLBACK_EXT
            auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)(vkGetInstanceProcAddr(s_VulkanInstance, "vkDestroyDebugReportCallbackEXT"));
            PG_ASSERT(vkDestroyDebugReportCallbackEXT != nullptr, "vkGetInstaceProcAddr returned null!");
            vkDestroyDebugReportCallbackEXT(s_VulkanInstance, m_DebugReportCallback, nullptr);
#endif

            auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkDestroyDebugUtilsMessengerEXT");
            PG_ASSERT(vkDestroyDebugUtilsMessengerEXT != nullptr, "vkGetInstanceProcAddr returned null!");
            vkDestroyDebugUtilsMessengerEXT(s_VulkanInstance, m_DebugUtilsMessenger, nullptr);
        }

        m_Device->Destroy();

        vkDestroyInstance(s_VulkanInstance, nullptr);
        s_VulkanInstance = nullptr;
    }

    void RendererContext::Init()
    {
        PG_ASSERT(glfwVulkanSupported(), "Vulkan is not supported with this GLFW!");

        if (!Utils::CheckDriverAPIVersionSupport(VK_API_VERSION_1_2))
        {
            MessageBox(nullptr, L"Incompatible Vulkan driver version. \nUpdate your GPU drivers!", L"vkPlayground Error", MB_OK | MB_ICONERROR);
            PG_ASSERT(false, "Incompatible vulkan driver!");
        }

        ////////////////////////////////////////////////////////
        ////// Application Info
        ////////////////////////////////////////////////////////
        // This is kind of optional but it may provide some information to the driver in order to its own optimizations magic
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "vkPlayground";
        appInfo.pEngineName = "vkPlayground";
        appInfo.apiVersion = VK_API_VERSION_1_3;

        ////////////////////////////////////////////////////////
        ////// Extensions and validation
        ////////////////////////////////////////////////////////

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
                // VK_KHR_surface - VK_KHR_win32_surface
        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Very little performance hit, can be used in Release
        // This is for more debugging utilities just like the name identifies. Check Out: https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_debug_utils.html
        if (s_Validation)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_debug_report.html
            instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME); // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_get_physical_device_properties2.html
        }

        VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT }; // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkValidationFeatureEnableEXT.html
        VkValidationFeaturesEXT features = {}; // this feature enables the output of warnings related to common misuse of the API, but which are not explicitly prohibited by the specification
        features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        features.enabledValidationFeatureCount = 1; // Number of features to enable, corresponds to the number of enum values in the array
        features.pEnabledValidationFeatures = enables; // pointer to an array specifying the validation features to be enabled.

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.pNext = nullptr; // &features
        // Set the desired global extensions since Vulkan is platform agnostic we need extensions to interface with glfw and others...
        instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

        // Here we check if VK_LAYER_KHRONOS_validation is supported by the vulkan by getting an array of supported layers via vkEnumerateInstanceLayerProperties and then comparing the string until the validation layer is found.
        // If the validation layer is not found then no validation layer will be created!
        if (s_Validation)
        {
            constexpr const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

            // Check if this layer is available at instance level!
            uint32_t instanceLayerCount;
            vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
            std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
            vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

            bool validationLayerSupported = false;
            std::cout << "[Renderer] Vulkan instance layers:\n";
            for (const VkLayerProperties& layer : instanceLayerProperties)
            {
                std::cout << "[Renderer] \t" << layer.layerName << std::endl;
                if (strcmp(layer.layerName, validationLayerName) == 0)
                {
                    validationLayerSupported = true;
                    break;
                }
            }

            if (validationLayerSupported)
            {
                instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
                instanceCreateInfo.enabledLayerCount = 1;
            }
            else
            {
                std::cerr << "[Renderer] Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled!" << std::endl;
            }
        }

        ////////////////////////////////////////////////////////
        ////// Instance and surface creation
        ////////////////////////////////////////////////////////
        VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_VulkanInstance));
        vkPlayground::Utils::VulkanLoadDebugUtilsExtensions(s_VulkanInstance);

        // Load the debug messenger and setup the callback
        if (s_Validation)
        {
#if PG_USE_DEBUG_REPORT_CALLBACK_EXT
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)(vkGetInstanceProcAddr(s_VulkanInstance, "vkCreateDebugReportCallbackEXT"));
            PG_ASSERT(vkCreateDebugReportCallbackEXT != nullptr, "vkGetInstanceProcAddr returned null!");

            VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {};
            debugReportCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debugReportCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debugReportCreateInfo.pfnCallback = Utils::VulkanDebugReportCallback;
            debugReportCreateInfo.pUserData = nullptr;

            VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(s_VulkanInstance, &debugReportCreateInfo, nullptr, &m_DebugReportCallback));
#endif

            auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkCreateDebugUtilsMessengerEXT");
            PG_ASSERT(vkCreateDebugUtilsMessengerEXT != nullptr, "vkGetInstanceProcAddr returned null!");

            VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {};
            debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                /*| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT*/;
            debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Callback is notified about all types of messages
            debugMessengerCreateInfo.pfnUserCallback = Utils::VulkanDebugCallback;
            debugMessengerCreateInfo.pUserData = nullptr; // Optional

            VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(s_VulkanInstance, &debugMessengerCreateInfo, nullptr, &m_DebugUtilsMessenger));
        }

        // Select and Create Physical Device
        m_PhysicalDevice = VulkanPhysicalDevice::Create();

        VkPhysicalDeviceFeatures enabledFeatures = {};
        // TODO: Add features as we go...
        m_Device = VulkanDevice::Create(m_PhysicalDevice, enabledFeatures);

        // TODO: VMA Init...

        // TODO: Pipeline caches...
    }

}