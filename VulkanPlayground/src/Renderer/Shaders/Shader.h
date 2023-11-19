#pragma once

#include "Core/Base.h"
#include "ShaderResources.h"
#include "ShaderUniform.h"

#include <vulkan/vulkan.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace vkPlayground {

	class Shader : public RefCountedObject
	{
	public:
		struct ReflectionData
		{
			std::vector<ShaderResources::ShaderDescriptorSet> ShaderDescriptorSets;
			// std::unordered_map<std::string, ShaderResourceDeclaration> Resources; // Mainly for all the types of images decriptors

			// TODO: Reflection data for Descriptor Sets, Resources(Uniform buffers, Storage buffers, Images and textures), Constant Buffers
			// and PushConstantRanges
		};

	public:
		Shader() = default;
		Shader(std::string_view path, bool forceCompile = false, bool disableOptimization = false);
		~Shader();

		[[nodiscard]] static Ref<Shader> Create();
		[[nodiscard]] static Ref<Shader> Create(std::string_view path, bool forceCompile = false, bool disableOptimization = false);

		void Reload();

		// Release the shader modules after the pipelines have been created
		void Release();

		std::size_t GetHash() const;
		const std::string_view GetName() const { return m_Name; }
		const std::filesystem::path& GetFilePath() const { return m_FilePath; }

		bool TryReadReflectionData(StreamReader* reader);
		void SerializeReflectionData(StreamWriter* serializer);
		void SetReflectionData(const ReflectionData& reflectionData) { m_ReflectionData = reflectionData; }

		// NOTE: To be used in the pipeline creation
		const std::vector<VkPipelineShaderStageCreateInfo>& GetPipelineShaderStageCreateInfos() const { return m_PipelineShaderStageCreateInfos; }

		VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) { return m_DescriptorSetLayouts.at(set); }
		std::vector<VkDescriptorSetLayout> GetAllDescriptorSetLayout();

		ShaderResources::UniformBuffer& GetUniformBuffer(const uint32_t set = 0, const uint32_t binding = 0);
		uint32_t GetUniformBufferCount(const uint32_t set = 0);

		const std::vector<ShaderResources::ShaderDescriptorSet>& GetShaderDescriptorSets() const { return m_ReflectionData.ShaderDescriptorSets; }
		bool HasDescriptorSet(uint32_t set) const { return m_DescriptorPoolTypeCounts.contains(set); }

		// For getting the descriptor mainly for materials in the renderer
		const VkWriteDescriptorSet* GetDescriptorSet(const std::string& name, uint32_t set = 0) const;

	private:
		void LoadAndCreateShaders(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData);
		void CreateDescriptors();

	private:
		std::string m_Name;
		std::filesystem::path m_FilePath;
		bool m_DisableOptimizations = false;

		std::map<VkShaderStageFlagBits, std::vector<uint32_t>> m_VulkanSPIRV;
		std::vector<VkPipelineShaderStageCreateInfo> m_PipelineShaderStageCreateInfos;

		ReflectionData m_ReflectionData;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::unordered_map<uint32_t, std::vector<VkDescriptorPoolSize>> m_DescriptorPoolTypeCounts;

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