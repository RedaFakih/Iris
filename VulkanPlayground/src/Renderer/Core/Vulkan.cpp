#include "vkPch.h"
#include "Vulkan.h"

namespace vkPlayground::Utils {

	void VulkanLoadDebugUtilsExtensions(VkInstance instance)
	{
		fpSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
		if (fpSetDebugUtilsObjectNameEXT == nullptr)
		{
			fpSetDebugUtilsObjectNameEXT = [](VkDevice, const VkDebugUtilsObjectNameInfoEXT*) -> VkResult { return VK_SUCCESS; };
		}

		fpCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
		if (fpCmdBeginDebugUtilsLabelEXT == nullptr)
		{
			fpCmdBeginDebugUtilsLabelEXT = [](VkCommandBuffer, const VkDebugUtilsLabelEXT*) -> void {};
		}

		fpCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
		if (fpCmdEndDebugUtilsLabelEXT == nullptr)
		{
			fpCmdEndDebugUtilsLabelEXT = [](VkCommandBuffer) -> void {};
		}

		fpCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)(vkGetInstanceProcAddr(instance, "vkCmdInsertDebugUtilsLabelEXT"));
		if (fpCmdInsertDebugUtilsLabelEXT == nullptr)
		{
			fpCmdInsertDebugUtilsLabelEXT = [](VkCommandBuffer, const VkDebugUtilsLabelEXT*) -> void {};
		}
	}

}