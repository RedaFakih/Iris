#pragma once

#include "Core/Base.h"
#include "Renderer/Core/VulkanAllocator.h"

#include <vulkan/vulkan.h>

#include <string>
#include <initializer_list>
#include <vector>

namespace vkPlayground {

	enum class ShaderDataType : uint8_t
	{
		None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Int2, Int3, Int4, Bool
	};

	static constexpr uint32_t ShaderDataTypeSize(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:    return 4;
			case ShaderDataType::Float2:   return 4 * 2;
			case ShaderDataType::Float3:   return 4 * 3;
			case ShaderDataType::Float4:   return 4 * 4;
			case ShaderDataType::Mat3:     return 4 * 3 * 3;
			case ShaderDataType::Mat4:     return 4 * 4 * 4;
			case ShaderDataType::Int:      return 4;
			case ShaderDataType::Int2:     return 4 * 2;
			case ShaderDataType::Int3:     return 4 * 3;
			case ShaderDataType::Int4:     return 4 * 4;
			case ShaderDataType::Bool:     return 1;
		}

		PG_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	struct VertexInputElement
	{
		std::string Name;
		ShaderDataType Type;
		uint32_t Size;
		uint32_t Offset;
		bool Normalized;

		constexpr VertexInputElement() = default;

		constexpr VertexInputElement(ShaderDataType type, const std::string& name, bool normalized = false)
			: Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
		{
		}

		constexpr uint32_t GetComponentCount() const
		{
			switch (Type)
			{
				case ShaderDataType::Float:		return 1;
				case ShaderDataType::Float2:	return 2;
				case ShaderDataType::Float3:	return 3;
				case ShaderDataType::Float4:	return 4;
				case ShaderDataType::Mat3:		return 3 * 3;
				case ShaderDataType::Mat4:		return 4 * 4;
				case ShaderDataType::Int:		return 1;
				case ShaderDataType::Int2:		return 2;
				case ShaderDataType::Int3:		return 3;
				case ShaderDataType::Int4:		return 4;
				case ShaderDataType::Bool:		return 1;
			}

			PG_ASSERT(false, "Unknown ShaderDataType!");
			return 0;
		}
	};

	class VertexInputLayout
	{
	public:
		VertexInputLayout() = default;
		VertexInputLayout(const std::initializer_list<VertexInputElement>& initList)
			: m_Elements(initList)
		{
			CalculateOffsetAndStride();
		}

		uint32_t GetStride() const { return m_Stride; }
		const std::vector<VertexInputElement>& GetElements() const { return m_Elements; }
		uint32_t GetElementCount() const { return (uint32_t)m_Elements.size(); }

		std::vector<VertexInputElement>::iterator begin() { return m_Elements.begin(); }
		std::vector<VertexInputElement>::iterator end() { return m_Elements.end(); }
		std::vector<VertexInputElement>::const_iterator begin() const { return m_Elements.cbegin(); }
		std::vector<VertexInputElement>::const_iterator end() const { return m_Elements.cend(); }

	private:
		void CalculateOffsetAndStride()
		{
			uint32_t offset = 0;
			for (VertexInputElement& element : m_Elements)
			{
				element.Offset = offset;
				offset += element.Size;
				m_Stride += element.Size;
			}
		}

	private:
		std::vector<VertexInputElement> m_Elements;
		uint32_t m_Stride = 0;
	};

	enum class VertexBufferUsage : uint8_t
	{
		None = 0, Static, Dynamic
	};

	class VertexBuffer : public RefCountedObject
	{
	public:
		// Create a buffer in DEVICE_LOCAL memory
		VertexBuffer(void* data, uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Static);
		// Create a buffer in HOST_VISIBLE memory
		VertexBuffer(uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Dynamic);
		~VertexBuffer();

		[[nodiscard]] static Ref<VertexBuffer> Create(void* data, uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Static);
		[[nodiscard]] static Ref<VertexBuffer> Create(uint32_t size, VertexBufferUsage usage = VertexBufferUsage::Static);

		// NOTE: Does not work if you created the buffer without giving it data directly
		void SetData(const void* data, uint32_t size, uint32_t offset = 0);

		uint32_t GetSize() const { return m_Size; }
		VkBuffer GetVulkanBuffer() const { return m_VulkanBuffer; }

	private:
		uint32_t m_Size = 0;

		VkBuffer m_VulkanBuffer = nullptr;
		VmaAllocation m_MemoryAllocation = nullptr;
	};

}