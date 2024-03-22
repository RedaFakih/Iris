#pragma once

#include "Core/Base.h"
#include "ShaderResources.h"
#include "ShaderUniform.h"

#include <vulkan/vulkan.h>

#include <map>
#include <set>
#include <string_view>

namespace vkPlayground {

	class Shader : public RefCountedObject
	{
	public:
		struct ReflectionData
		{
			std::vector<ShaderResources::ShaderDescriptorSet> ShaderDescriptorSets;
			// The two below more or less reference the same thing but serve different purposes
			std::unordered_map<std::string, ShaderBuffer> ConstantBuffers; // These are used for material uniform creation
			std::vector<ShaderResources::PushConstantRange> PushConstantRanges; // These are used for the pipeline creation
		};

	public:
		Shader() = default;
		Shader(std::string_view path, bool forceCompile = false, bool disableOptimization = false);
		~Shader();

		[[nodiscard]] static Ref<Shader> Create();
		[[nodiscard]] static Ref<Shader> Create(std::string_view path, bool forceCompile = false, bool disableOptimization = false);

		void Reload();

		void Release();

		std::size_t GetHash() const;
		const std::string_view GetName() const { return m_Name; }
		const std::string& GetFilePath() const { return m_FilePath; }

		bool TryReadReflectionData(StreamReader* reader);
		void SerializeReflectionData(StreamWriter* serializer);
		void SetReflectionData(const ReflectionData& reflectionData) { m_ReflectionData = reflectionData; }

		// NOTE: To be used in the pipeline creation
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }

		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) { return m_DescriptorSetLayouts.at(set); }
		std::vector<VkDescriptorSetLayout> GetAllDescriptorSetLayouts();

		ShaderResources::UniformBuffer& GetUniformBuffer(const uint32_t set = 0, const uint32_t binding = 0);
		uint32_t GetUniformBufferCount(const uint32_t set = 0);

		const std::vector<ShaderResources::ShaderDescriptorSet>& GetShaderDescriptorSets() const { return m_ReflectionData.ShaderDescriptorSets; }
		bool HasDescriptorSet(uint32_t set) const { return m_ExistingSets.contains(set); }
		const std::unordered_map<std::string, ShaderBuffer>& GetShaderBuffers() const { return m_ReflectionData.ConstantBuffers; }
		const std::vector<ShaderResources::PushConstantRange>& GetPushConstantRanges() const { return m_ReflectionData.PushConstantRanges; }

		const std::unordered_map<VkDescriptorType, uint32_t>& GetDescriptorPoolSizes() const { return m_DescriptorPoolTypeCounts; }

		// For getting the descriptor mainly for materials in the renderer
		const VkWriteDescriptorSet* GetDescriptorSet(const std::string& name, uint32_t set = 0) const;

	private:
		void LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void BuildWriteDescriptors();

	private:
		std::string m_Name;
		std::string m_FilePath;
		bool m_DisableOptimizations = false;

		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> m_VulkanSPIRV;
		std::vector<VkPipelineShaderStageCreateInfo> m_PipelineShaderStageCreateInfos;

		ReflectionData m_ReflectionData;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::set<uint32_t> m_ExistingSets;

		// Global pool sizes that are needed for the global descriptor pool instead of creating per-set descriptor pools since that is inefficient
		std::unordered_map<VkDescriptorType, uint32_t> m_DescriptorPoolTypeCounts;

		friend class ShaderCompiler;
	};

	class ShadersLibrary : public RefCountedObject
	{
	public:
		ShadersLibrary() = default;
		~ShadersLibrary() = default;

		[[nodiscard]] static Ref<ShadersLibrary> Create();

		void Add(Ref<Shader> shader);
		void Load(std::string_view filePath, bool forceCompile = false, bool disableOptimizations = false);
		void Load(std::string_view name, std::string_view filePath);
		const Ref<Shader> Get(const std::string& name) const;

		std::size_t GetSize() const noexcept { return m_Library.size(); }

		std::unordered_map<std::string, Ref<Shader>>& GetShaders() { return m_Library; }
		const std::unordered_map<std::string, Ref<Shader>>& GetShaders() const { return m_Library; }

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Library;
	};

}