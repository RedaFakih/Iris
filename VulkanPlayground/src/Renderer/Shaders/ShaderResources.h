#pragma once

#include "Serialization/FileStream.h"

#include <vulkan/vulkan.h>

#include <string>

namespace vkPlayground::ShaderResources {

	struct UniformBuffer
	{
		std::string Name;
		VkDescriptorBufferInfo Descriptor;
		uint32_t Size = 0;
		uint32_t BindingPoint = 0;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void Serialize(StreamWriter* stream, const UniformBuffer& instance)
		{
			stream->WriteString(instance.Name);
			stream->WriteRaw(instance.Descriptor);
			stream->WriteRaw(instance.Size);
			stream->WriteRaw(instance.BindingPoint);
			stream->WriteRaw(instance.ShaderStage);
		}

		static void Deserialize(StreamReader* stream, UniformBuffer& instance)
		{
			stream->ReadString(instance.Name);
			stream->ReadRaw(instance.Descriptor);
			stream->ReadRaw(instance.Size);
			stream->ReadRaw(instance.BindingPoint);
			stream->ReadRaw(instance.ShaderStage);
		}
	};

	struct ShaderDescriptorSet
	{
		std::unordered_map<uint32_t, UniformBuffer> UniformBuffers; // binding -> uniform buffer

		std::unordered_map<std::string, VkWriteDescriptorSet> WriteDescriptorSets;

		operator bool() const { return !(UniformBuffers.empty()); }
	};

	// StorageBuffer...?
	// ImageSampler...?
	// PushConstantRange...?
}