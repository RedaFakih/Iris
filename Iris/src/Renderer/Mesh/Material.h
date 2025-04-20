#pragma once

#include "Core/Buffer.h"
#include "Renderer/DescriptorSetManager.h"
#include "Renderer/Shaders/Shader.h"
#include "Renderer/Shaders/ShaderUniform.h"

namespace Iris {

	/*
	 * This material object handles objects and any resource found in set = 3 in the shaders
	 */

	enum class MaterialFlag
	{
		None		  = BIT(0), // 0b00001
		DepthTest	  = BIT(1), // 0b00010
		Blend		  = BIT(2), // 0b00100
		TwoSided	  = BIT(3), // 0b01000
		ShadowCasting = BIT(4)  // 0b10000
	};

	class Material : public RefCountedObject
	{
	public:
		Material(Ref<Shader> shader, const std::string& name = "");
		Material(Ref<Material> other, const std::string& name = "");
		virtual ~Material();

		[[nodiscard]] inline static Ref<Material> Create(Ref<Shader> shader, const std::string& name = "")
		{
			return CreateRef<Material>(shader, name);
		}

		[[nodiscard]] inline static Ref<Material> Create(Ref<Material> other, const std::string& name = "")
		{
			return CreateRef<Material>(other, name);
		}

		void Prepare();
		void OnShaderReloaded();

		void Set(std::string_view name, int value);
		void Set(std::string_view name, uint32_t value);
		void Set(std::string_view name, float value);
		void Set(std::string_view name, bool value);
		void Set(std::string_view name, const glm::ivec2& value);
		void Set(std::string_view name, const glm::ivec3& value);
		void Set(std::string_view name, const glm::ivec4& value);
		void Set(std::string_view name, const glm::vec2& value);
		void Set(std::string_view name, const glm::vec3& value);
		void Set(std::string_view name, const glm::vec4& value);
		void Set(std::string_view name, const glm::mat3& value);
		void Set(std::string_view name, const glm::mat4& value);

		void Set(std::string_view name, const Ref<Texture2D>& value);
		void Set(std::string_view name, const Ref<Texture2D>& value, uint32_t arrayIndex);
		void Set(std::string_view name, const Ref<TextureCube>& value);
		void Set(std::string_view name, const Ref<ImageView>& imageView);

		template<typename T>
		void Set(std::string_view name, const T& value)
		{
			const ShaderUniform* decl = FindUniformDeclaration(name);
			IR_ASSERT(decl, "Could not find uniform!");
			if (!decl)
				return;

			m_UniformStorageBuffer.Write(reinterpret_cast<const uint8_t*>(&value), decl->GetSize(), decl->GetOffset());
		}

		int& GetInt(std::string_view name);
		uint32_t& GetUInt(std::string_view name);
		float& GetFloat(std::string_view name);
		bool& GetBool(std::string_view name);
		glm::ivec2& GetIVector2(std::string_view name);
		glm::ivec3& GetIVector3(std::string_view name);
		glm::ivec4& GetIVector4(std::string_view name);
		glm::vec2& GetVector2(std::string_view name);
		glm::vec3& GetVector3(std::string_view name);
		glm::vec4& GetVector4(std::string_view name);
		glm::mat3& GetMatrix3(std::string_view name);
		glm::mat4& GetMatrix4(std::string_view name);

		Ref<Texture2D> GetTexture2D(std::string_view name);
		Ref<Texture2D> TryGetTexture2D(std::string_view name);

		Ref<TextureCube> GetTextureCube(std::string_view name);
		Ref<TextureCube> TryGetTextureCube(std::string_view name);

		template<typename T>
		T& Get(std::string_view name)
		{
			const ShaderUniform* decl = FindUniformDeclaration(name);
			IR_ASSERT(decl, "Could not find uniform!");
			return m_UniformStorageBuffer.Read<T>(decl->GetOffset());
		}

		template<typename T>
		Ref<T> TryGetResource(std::string_view name)
		{
			return m_DescriptorSetManager.GetInput<T>(name);
		}

		uint32_t GetMaterialFlags() const { return m_MaterialFlags; }
		bool GetFlag(MaterialFlag flag) const { return static_cast<uint32_t>(flag) & m_MaterialFlags; }
		void SetFlags(uint32_t flags) { m_MaterialFlags = flags; }
		void SetFlag(MaterialFlag flag, bool value = true) 
		{
			if (value)
			{
				m_MaterialFlags |= static_cast<uint32_t>(flag);
			}
			else
			{
				m_MaterialFlags &= ~static_cast<uint32_t>(flag);
			}
		}

		VkDescriptorSet GetDescriptorSet(uint32_t frame)
		{
			if (m_DescriptorSetManager.GetFirstSetIndex() == UINT32_MAX)
				return nullptr;

			Prepare();
			return m_DescriptorSetManager.GetDescriptorSets(frame)[0];
		}

		uint32_t GetFirstSetIndex() const { return m_DescriptorSetManager.GetFirstSetIndex(); }

		Ref<Shader> GetShader() { return m_Shader; }
		const std::string& GetName() const { return m_Name; }
		Buffer GetUniformStorageBuffer() { return m_UniformStorageBuffer; }

	private:
		void Init(bool triggerCopy = false, const std::map<uint32_t, std::map<uint32_t, RenderPassInput>>& inputResources = std::map<uint32_t, std::map<uint32_t, RenderPassInput>>());
		void AllocateStorage();

		void SetVulkanDescriptor(std::string_view name, const Ref<Texture2D>& texture);
		void SetVulkanDescriptor(std::string_view name, const Ref<Texture2D>& texture, uint32_t arrayIndex);
		void SetVulkanDescriptor(std::string_view name, const Ref<TextureCube>& texture);
		void SetVulkanDescriptor(std::string_view name, const Ref<ImageView>& imageView);

		const ShaderUniform* FindUniformDeclaration(std::string_view name) const;

	private:
		std::string m_Name;
		Ref<Shader> m_Shader;

		DescriptorSetManager m_DescriptorSetManager;

		uint32_t m_MaterialFlags = 0;

		// Stores all the uniform data
		Buffer m_UniformStorageBuffer;
	};

}