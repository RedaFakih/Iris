#pragma once

#include "Serialization/FileStream.h"

#include <spirv-headers/spirv.hpp>
#include <vulkan/vulkan.h>

#include <string>

namespace Iris::ShaderResources {

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

	struct StorageBuffer
	{
		std::string Name;
		VkDescriptorBufferInfo Descriptor;
		uint32_t Size = 0;
		uint32_t BindingPoint = 0;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void Serialize(StreamWriter* stream, const StorageBuffer& instance)
		{
			stream->WriteString(instance.Name);
			stream->WriteRaw(instance.Descriptor);
			stream->WriteRaw(instance.Size);
			stream->WriteRaw(instance.BindingPoint);
			stream->WriteRaw(instance.ShaderStage);
		}

		static void Deserialize(StreamReader* stream, StorageBuffer& instance)
		{
			stream->ReadString(instance.Name);
			stream->ReadRaw(instance.Descriptor);
			stream->ReadRaw(instance.Size);
			stream->ReadRaw(instance.BindingPoint);
			stream->ReadRaw(instance.ShaderStage);
		}
	};

	struct ImageSampler
	{
		std::string Name;
		uint32_t DescriptorSet = 0;
		uint32_t BindingPoint = 0;
		spv::Dim Dimension = spv::Dim::DimMax; // Dimension of the Image Sampler (1D, 2D, 3D, Cube)
		uint32_t ArraySize = 0;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void Serialize(StreamWriter* stream, const ImageSampler& instance)
		{
			stream->WriteString(instance.Name);
			stream->WriteRaw(instance.DescriptorSet);
			stream->WriteRaw(instance.BindingPoint);
			stream->WriteRaw(instance.Dimension);
			stream->WriteRaw(instance.ArraySize);
			stream->WriteRaw(instance.ShaderStage);
		}

		static void Deserialize(StreamReader* stream, ImageSampler& instance)
		{
			stream->ReadString(instance.Name);
			stream->ReadRaw(instance.DescriptorSet);
			stream->ReadRaw(instance.BindingPoint);
			stream->ReadRaw(instance.Dimension);
			stream->ReadRaw(instance.ArraySize);
			stream->ReadRaw(instance.ShaderStage);
		}
	};

	struct PushConstantRange
	{
		uint32_t Size = 0;
		uint32_t Offset = 0;
		VkShaderStageFlagBits ShaderStage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

		static void Serialize(StreamWriter* stream, const PushConstantRange& instance) { stream->WriteRaw(instance); }
		static void Deserialize(StreamReader* stream, PushConstantRange& instance) { stream->ReadRaw(instance); }
	};

	struct ShaderDescriptorSet
	{
		std::unordered_map<uint32_t, UniformBuffer> UniformBuffers; // binding -> uniform buffer
		std::unordered_map<uint32_t, StorageBuffer> StorageBuffers; // binding -> storage buffer
		std::unordered_map<uint32_t, ImageSampler> ImageSamplers;
		std::unordered_map<uint32_t, ImageSampler> StorageImages;

		std::unordered_map<std::string, VkWriteDescriptorSet> WriteDescriptorSets;

		operator bool() const { return !(UniformBuffers.empty() && ImageSamplers.empty() && StorageBuffers.empty() && StorageImages.empty()); }
	};

}