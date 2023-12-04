#include "vkPch.h"
#include "RendererContext.h"

#include "Core/Application.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Core/VulkanAllocator.h"
#include "Renderer/Renderer.h"

#include <glfw/glfw3.h>

// Vulkan version 1.3 forced.
#ifndef VK_API_VERSION_1_3
    #error Wrong vulkan SDK!
#endif

namespace vkPlayground {

#ifdef PLAYGROUND_DEBUG
   static  bool s_Validation = true;
#else
    static bool s_Validation = false;
#endif

    namespace Utils {

        static bool CheckDriverAPIVersionSupport(uint32_t minimumSupportedVersion)
        {
            uint32_t instanceVersion = 0;
            vkEnumerateInstanceVersion(&instanceVersion);

            if (instanceVersion < minimumSupportedVersion)
            {
                PG_CORE_CRITICAL_TAG("Renderer", "Incompatible driver version!");
                PG_CORE_CRITICAL_TAG("Renderer", "\tYou have: {0}.{1}.{2}", VK_API_VERSION_MAJOR(instanceVersion), VK_API_VERSION_MINOR(instanceVersion), VK_API_VERSION_PATCH(instanceVersion));
                PG_CORE_CRITICAL_TAG("Renderer", "\tYou need at least: {0}.{1}.{2}", VK_API_VERSION_MAJOR(minimumSupportedVersion), VK_API_VERSION_MINOR(minimumSupportedVersion), VK_API_VERSION_PATCH(minimumSupportedVersion));

                return false;
            }

            return true;
        }

        constexpr const char* VkDebugUtilsMessageType(const VkDebugUtilsMessageTypeFlagsEXT type)
        {
            switch (type)
            {
                case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:		return "General";
                case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:	return "Validation";
                case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:	return "Performance";
            }

            return "Unknown";
        }

        constexpr const char* VkDebugUtilsMessageSeverity(const VkDebugUtilsMessageSeverityFlagBitsEXT severity)
        {
            switch (severity)
            {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:		return "Error";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:	return "Warning";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:		return "Info";
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:	return "Verbose";
            }

            return "Unknown";
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData)
        {
            (void)pUserData;

            std::string labels, objects;
            if (pCallbackData->cmdBufLabelCount)
            {
                labels = fmt::format("\tLabels ({0}): \n", pCallbackData->cmdBufLabelCount);
                for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
                {
                    const VkDebugUtilsLabelEXT& label = pCallbackData->pCmdBufLabels[i];
                    const std::string colorStr = fmt::format("[{}, {}, {}, {}]", label.color[0], label.color[1], label.color[2], label.color[3]);
                    labels.append(fmt::format("\t\t- Command buffer label[{0}]: Name: {1} Color: {2}\n", i, label.pLabelName ? label.pLabelName : "NULL", colorStr));
                }
            }

            if (pCallbackData->objectCount)
            {
                objects = fmt::format("\tObjects ({0}): \n", pCallbackData->objectCount);
                for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
                {
                    const VkDebugUtilsObjectNameInfoEXT& object = pCallbackData->pObjects[i];
                    objects.append(fmt::format("\t\t- Object[{0}]: Name: {1} Type: {2} Handle: {3:#x}\n", i, object.pObjectName ? object.pObjectName : "NULL", Utils::VkObjectTypeToString(object.objectType), object.objectHandle));
                }
            }

            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
                PG_CORE_WARN_TAG("ValidationLayers", "Type: {0}, Severity: {1}\nMessage: \n\t{2}\n {3}, {4}",
                    VkDebugUtilsMessageType(messageType), 
                    VkDebugUtilsMessageSeverity(messageSeverity), 
                    pCallbackData->pMessage, 
                    labels, 
                    objects);
            else
                PG_CORE_ERROR_TAG("ValidationLayers", "Type: {0}, Severity: {1}\nMessage: \n\t{2}\n {3}, {4}", 
                    VkDebugUtilsMessageType(messageType), 
                    VkDebugUtilsMessageSeverity(messageSeverity), 
                    pCallbackData->pMessage, 
                    labels, 
                    objects);

            return VK_FALSE;
        }

    }

    VkInstance RendererContext::s_VulkanInstance = nullptr;

    Ref<RendererContext> RendererContext::Create()
    {
        return CreateRef<RendererContext>();
    }

    RendererContext::RendererContext()
    {
    }

    RendererContext::~RendererContext()
    {
        if (s_Validation)
        {
            auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkDestroyDebugUtilsMessengerEXT");
            PG_ASSERT(vkDestroyDebugUtilsMessengerEXT != nullptr, "vkGetInstanceProcAddr returned null!");
            vkDestroyDebugUtilsMessengerEXT(s_VulkanInstance, m_DebugUtilsMessenger, nullptr);
        }

        // NOTE: Its too late to destroy the device here, Destroy() asks for the context that we are in the middle of destroying
        // Device is destroyed in Window::Shutdown;
        // m_Device->Destroy();

        // NOTE: We cant destroy the allocator here since it need the Device to destroy all the memory it allocates so we need to destroy it
        // before destroying the Device. So in Window::Shutdown
        // VulkanAllocator::ShutDown();

        vkDestroyInstance(s_VulkanInstance, nullptr);
        s_VulkanInstance = nullptr;
    }

    void RendererContext::Init()
    {
        PG_ASSERT(glfwVulkanSupported(), "Vulkan is not supported with this GLFW!");

        if (!Utils::CheckDriverAPIVersionSupport(VK_API_VERSION_1_2))
        {
            PG_ASSERT(false, "Incompatible vulkan driver!");
        }

        ////////////////////////////////////////////////////////
        ////// Application Info
        ////////////////////////////////////////////////////////
        // This is kind of optional but it may provide some information to the driver in order to its own optimizations magic
        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "vkPlayground",
            .pEngineName = "vkPlayground",
            .apiVersion = VK_API_VERSION_1_3
        };

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
            // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_debug_report.html
            instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_get_physical_device_properties2.html
            instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkValidationFeatureEnableEXT.html
        VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
        // this feature enables the output of warnings related to common misuse of the API, but which are not explicitly prohibited by the specification
        VkValidationFeaturesEXT features = {
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .enabledValidationFeatureCount = 2,
            .pEnabledValidationFeatures = enables
        };

        // Set the desired global extensions since Vulkan is platform agnostic we need extensions to interface with glfw and others...
        VkInstanceCreateInfo instanceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            // .pNext = nullptr, // NOTE: If we dont want to know all the best practices and synchronization mistakes... could just disable
            .pNext = &features,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
            .ppEnabledExtensionNames = instanceExtensions.data()
        };

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
            PG_CORE_INFO_TAG("Renderer", "Vulkan instance layers:");
            for (const VkLayerProperties& layer : instanceLayerProperties)
            {
                PG_CORE_INFO_TAG("Renderer", "\t{0}", layer.layerName);
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
                PG_CORE_ERROR_TAG("Renderer", "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled!");
            }
        }

        ////////////////////////////////////////////////////////
        ////// Instance and surface creation
        ////////////////////////////////////////////////////////
        VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &s_VulkanInstance));
        Utils::VulkanLoadDebugUtilsExtensions(s_VulkanInstance);

        // Load the debug messenger and setup the callback
        if (s_Validation)
        {
            auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_VulkanInstance, "vkCreateDebugUtilsMessengerEXT");
            PG_ASSERT(vkCreateDebugUtilsMessengerEXT != nullptr, "vkGetInstanceProcAddr returned null!");

            VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT /* | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT */,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = Utils::VulkanDebugCallback,
                .pUserData = nullptr // optional
            };

            VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(s_VulkanInstance, &debugMessengerCreateInfo, nullptr, &m_DebugUtilsMessenger));
        }

        // Select and Create Physical Device
        m_PhysicalDevice = VulkanPhysicalDevice::Create();

        VkPhysicalDeviceFeatures enabledFeatures = {
            .independentBlend = true,
            .fillModeNonSolid = true,
            .wideLines = true,
            .samplerAnisotropy = true,
            .pipelineStatisticsQuery = true,
        };
        m_Device = VulkanDevice::Create(m_PhysicalDevice, enabledFeatures);

        VulkanAllocator::Init();
    }

    Ref<RendererContext> RendererContext::Get()
    {
        return Ref<RendererContext>(Renderer::GetContext());
    }

}