#include "IrisPCH.h"
#include "Material.h"

#include "Renderer/Renderer.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBufferSet.h"

namespace Iris {

	Ref<Material> Material::Create(Ref<Shader> shader, const std::string& name)
	{
		return CreateRef<Material>(shader, name);
	}

	Ref<Material> Material::Create(Ref<Material> other, const std::string& name)
	{
		return CreateRef<Material>(other, name);
	}

	Material::Material(Ref<Shader> shader, const std::string& name)
		: m_Shader(shader), m_Name(name)
	{
		Init(false, {});
		Renderer::RegisterShaderDependency(shader, this);
	}

	Material::Material(Ref<Material> other, const std::string& name)
		: m_Shader(other->GetShader()), m_Name(name)
	{
		if (name.empty())
			m_Name = other->GetName();

		Init(true, other->m_DescriptorSetManager.GetInputResources());
		Renderer::RegisterShaderDependency(m_Shader, this);

		m_UniformStorageBuffer = Buffer::Copy(other->m_UniformStorageBuffer.Data, other->m_UniformStorageBuffer.Size);
	}

	Material::~Material()
	{
		m_UniformStorageBuffer.Release();
	}

	void Material::Prepare()
	{
		m_DescriptorSetManager.InvalidateAndUpdate();
	}

	void Material::OnShaderReloaded()
	{
		// Init();
	}

	void Material::Set(std::string_view name, int value)
	{
		Set<int>(name, value);
	}

	void Material::Set(std::string_view name, uint32_t value)
	{
		Set<uint32_t>(name, value);
	}

	void Material::Set(std::string_view name, float value)
	{
		Set<float>(name, value);
	}

	void Material::Set(std::string_view name, bool value)
	{
		Set<int>(name, value); // Bools are 4 byte ints...
	}

	void Material::Set(std::string_view name, const glm::ivec2& value)
	{
		Set<glm::ivec2>(name, value);
	}

	void Material::Set(std::string_view name, const glm::ivec3& value)
	{
		Set<glm::ivec3>(name, value);
	}

	void Material::Set(std::string_view name, const glm::ivec4& value)
	{
		Set<glm::ivec4>(name, value);
	}

	void Material::Set(std::string_view name, const glm::vec2& value)
	{
		Set<glm::vec2>(name, value);
	}

	void Material::Set(std::string_view name, const glm::vec3& value)
	{
		Set<glm::vec3>(name, value);
	}

	void Material::Set(std::string_view name, const glm::vec4& value)
	{
		Set<glm::vec4>(name, value);
	}

	void Material::Set(std::string_view name, const glm::mat3& value)
	{
		Set<glm::mat3>(name, value);
	}

	void Material::Set(std::string_view name, const glm::mat4& value)
	{
		Set<glm::mat4>(name, value);
	}

	void Material::Set(std::string_view name, Ref<Texture2D> value)
	{
		SetVulkanDescriptor(name, value);
	}

	void Material::Set(std::string_view name, Ref<Texture2D> value, uint32_t arrayIndex)
	{
		SetVulkanDescriptor(name, value, arrayIndex);
	}

	int& Material::GetInt(std::string_view name)
	{
		return Get<int>(name);
	}

	uint32_t& Material::GetUInt(std::string_view name)
	{
		return Get<uint32_t>(name);
	}

	float& Material::GetFloat(std::string_view name)
	{
		return Get<float>(name);
	}

	bool& Material::GetBool(std::string_view name)
	{
		return Get<bool>(name);
	}

	glm::ivec2& Material::GetIVector2(std::string_view name)
	{
		return Get<glm::ivec2>(name);
	}

	glm::ivec3& Material::GetIVector3(std::string_view name)
	{
		return Get<glm::ivec3>(name);
	}

	glm::ivec4& Material::GetIVector4(std::string_view name)
	{
		return Get<glm::ivec4>(name);
	}

	glm::vec2& Material::GetVector2(std::string_view name)
	{
		return Get<glm::vec2>(name);
	}

	glm::vec3& Material::GetVector3(std::string_view name)
	{
		return Get<glm::vec3>(name);
	}

	glm::vec4& Material::GetVector4(std::string_view name)
	{
		return Get<glm::vec4>(name);
	}

	glm::mat3& Material::GetMatrix3(std::string_view name)
	{
		return Get<glm::mat3>(name);
	}

	glm::mat4& Material::GetMatrix4(std::string_view name)
	{
		return Get<glm::mat4>(name);
	}

	Ref<Texture2D> Material::GetTexture2D(std::string_view name)
	{
		return TryGetResource<Texture2D>(name);
	}

	Ref<Texture2D> Material::TryGetTexture2D(std::string_view name)
	{
		return TryGetResource<Texture2D>(name);
	}

	Ref<TextureCube> Material::GetTextureCube(std::string_view name)
	{
		return TryGetResource<TextureCube>(name);
	}

	Ref<TextureCube> Material::TryGetTextureCube(std::string_view name)
	{
		return TryGetResource<TextureCube>(name);
	}

	void Material::Init(bool triggerCopy, const std::map<uint32_t, std::map<uint32_t, RenderPassInput>>& inputResources)
	{
		AllocateStorage();

		m_MaterialFlags |= static_cast<uint32_t>(MaterialFlag::Blend);
		m_MaterialFlags |= static_cast<uint32_t>(MaterialFlag::DepthTest);

		DescriptorSetManagerSpecification spec = {
			.Shader = m_Shader,
			.DebugName = m_Name.empty() ? fmt::format("{} (Material)", m_Shader->GetName()) : m_Name,
			.DefaultResources = true,
			.StartingSet = 3,
			.EndingSet = 3,
			// NOTE: This way we could either trigger a DescriptorSetManager copy internally or
			// trigger creation of a new descriptor set manager based on input resources in the provided shader if we provide an empty ExistingInputResources
			.TriggerCopy = triggerCopy,
			.ExistingInputResources = inputResources
		};
		m_DescriptorSetManager = DescriptorSetManager(spec);

		m_DescriptorSetManager.Bake();
	}

	void Material::AllocateStorage()
	{
		const std::unordered_map<std::string, ShaderBuffer>& shaderBuffers = m_Shader->GetShaderBuffers();
		
		if (shaderBuffers.size() > 0)
		{
			uint32_t size = 0;
			for (auto [name, shaderBuffer] : shaderBuffers)
				size += shaderBuffer.Size;

			m_UniformStorageBuffer.Allocate(size);
			std::memset(m_UniformStorageBuffer.Data, 0, size);
		}
	}

	void Material::SetVulkanDescriptor(std::string_view name, const Ref<Texture2D>& texture)
	{
		m_DescriptorSetManager.SetInput(name, texture);
	}

	void Material::SetVulkanDescriptor(std::string_view name, const Ref<Texture2D>& texture, uint32_t arrayIndex)
	{
		m_DescriptorSetManager.SetInput(name, texture, arrayIndex);
	}

	const ShaderUniform* Material::FindUniformDeclaration(std::string_view name) const
	{
		const std::unordered_map<std::string, ShaderBuffer>& shaderBuffers = m_Shader->GetShaderBuffers();
		IR_ASSERT(shaderBuffers.size() <= 1, "Currently only ONE material buffer");

		if (shaderBuffers.size() > 0)
		{
			const ShaderBuffer& buffer = shaderBuffers.begin()->second;
			if (!buffer.Uniforms.contains(std::string(name)))
				return nullptr;

			return &buffer.Uniforms.at(std::string(name));
		}

		return nullptr;
	}

}