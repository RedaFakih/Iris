#include "RendererContext.h"

#include "Vulkan.h"

#include <glfw/glfw3.h>

#include <vector>
#include <Windows.h> // MessageBox

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
                std::cerr << "Incompatable Vulkan driver version!" << std::endl;
                std::cerr << "\tYou have: " << VK_API_VERSION_MAJOR(instanceVersion) << " " << VK_API_VERSION_MINOR(instanceVersion) << " " << VK_API_VERSION_PATCH(instanceVersion) << std::endl;
                std::cerr << "\tYou need at least: " << VK_API_VERSION_MAJOR(minimumSupportedVersion) << " " << VK_API_VERSION_MINOR(minimumSupportedVersion) << " " << VK_API_VERSION_PATCH(minimumSupportedVersion) << std::endl;

                return false;
            }

            return true;
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
        VkValidationFeaturesEXT features = {};
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
            uint32_t instanceLayerCount = 0;
            vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
            std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
            vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

            bool validationLayerSupported = false;
            std::cout << "Vulkan instance layers:" << std::endl;
            for (const VkLayerProperties& layer : instanceLayerProperties)
            {
                std::cout << "[Renderer]    " << layer.layerName << std::endl;
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
        // TODO: Load Debug Utils Extensions...

        // Load the debug messenger and setup the callback
        if (s_Validation)
        {

        }
    }

}