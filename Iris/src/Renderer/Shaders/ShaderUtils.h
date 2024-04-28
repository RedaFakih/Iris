#pragma once

#include "Core/Base.h"

#include <vulkan/vulkan_core.h>
#include <shaderc/shaderc.hpp>

namespace Iris::ShaderUtils {

	inline static constexpr const char* VkShaderStageToMacroString(const VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:    return "__VERTEX__";
		case VK_SHADER_STAGE_FRAGMENT_BIT:  return "__FRAGMENT__";
		case VK_SHADER_STAGE_COMPUTE_BIT:   return "__COMPUTE__";
		}

		IR_ASSERT(false);
		return "INVALID";
	}

	inline static constexpr const char* VkShaderStageToString(const VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT:    return "Vertex";
			case VK_SHADER_STAGE_FRAGMENT_BIT:  return "Fragment";
			case VK_SHADER_STAGE_COMPUTE_BIT:   return "Compute";
		}

		IR_ASSERT(false);
		return "INVALID";
	}

	inline static constexpr const char* ShadercShaderStageToString(shaderc_shader_kind type)
	{
		switch (type)
		{
			case shaderc_vertex_shader:   return "Vertex";
			case shaderc_fragment_shader: return "Fragment";
			case shaderc_compute_shader:  return "Compute";
		}

		IR_ASSERT(false);
		return "INVALID";
	}

	inline static constexpr VkShaderStageFlagBits VkShaderStageFromString(const std::string_view type)
	{
		if (type == "vertex" || type == "Vertex")	    return VK_SHADER_STAGE_VERTEX_BIT;
		if (type == "fragment" || type == "Fragment")	return VK_SHADER_STAGE_FRAGMENT_BIT;
		if (type == "compute" || type == "Compute")	    return VK_SHADER_STAGE_COMPUTE_BIT;

		return VK_SHADER_STAGE_ALL;
	}

	inline static constexpr shaderc_shader_kind ShadercShaderStageFromString(const std::string_view type)
	{
		if (type == "vertex")    return shaderc_vertex_shader;
		if (type == "fragment")  return shaderc_fragment_shader;
		if (type == "compute")   return shaderc_compute_shader;

		IR_ASSERT(false);
		return {};
	}

	inline static constexpr shaderc_shader_kind VkShaderStageToShaderc(const VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
			case VK_SHADER_STAGE_VERTEX_BIT:    return shaderc_vertex_shader;
			case VK_SHADER_STAGE_FRAGMENT_BIT:  return shaderc_fragment_shader;
			case VK_SHADER_STAGE_COMPUTE_BIT:   return shaderc_compute_shader;
		}

		IR_ASSERT(false);
		return {};
	}

	inline static constexpr VkShaderStageFlagBits VkShaderStageFromShaderc(shaderc_shader_kind type)
	{
		switch (type)
		{
			case shaderc_compute_shader:	return VK_SHADER_STAGE_COMPUTE_BIT;
			case shaderc_vertex_shader:		return VK_SHADER_STAGE_VERTEX_BIT;
			case shaderc_fragment_shader:   return VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		IR_ASSERT(false);
		return {};
	}

	inline static constexpr const char* ShaderStageCachedFileExtension(const VkShaderStageFlagBits stage, bool debug)
	{
		if (debug)
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:    return ".cachedVulkanDebug.Vert";
				case VK_SHADER_STAGE_FRAGMENT_BIT:  return ".cachedVulkanDebug.Frag";
				case VK_SHADER_STAGE_COMPUTE_BIT:   return ".cachedVulkanDebug.Comp";
			}
		}
		else
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_VERTEX_BIT:    return ".cachedVulkan.Vert";
				case VK_SHADER_STAGE_FRAGMENT_BIT:  return ".cachedVulkan.Frag";
				case VK_SHADER_STAGE_COMPUTE_BIT:   return ".cachedVulkan.Comp";
			}
		}

		IR_ASSERT(false);
		return "";
	}

}