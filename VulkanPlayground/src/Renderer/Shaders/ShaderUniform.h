#pragma once

#include <vector>
#include <string>

namespace vkPlayground {

	enum class ShaderUniformType : uint8_t
	{
		None = 0,
		Bool, Int, UInt, Float, 
		Vec2, Vec3, Vec4, IVec2, IVec3, IVec4,
		Mat3, Mat4
	};

	class ShaderUniform
	{
	public:
		ShaderUniform() = default;
		ShaderUniform(const std::string& name, ShaderUniformType type, uint32_t size, uint32_t offset)
			: m_Name(name), m_Type(type), m_Size(size), m_Offset(offset)
		{
		}

		const std::string& GetName() const { return m_Name; }
		ShaderUniformType GetType() const { return m_Type; }
		uint32_t GetSize() const { return m_Size; }
		uint32_t GetOffset() const { return m_Offset; }

		inline static constexpr std::string_view UniformTypeToString(ShaderUniformType type)
		{
			switch (type)
			{
				case ShaderUniformType::Bool:	return "Bool";
				case ShaderUniformType::Int:	return "Int";
				case ShaderUniformType::UInt:	return "UInt";
				case ShaderUniformType::Float:	return "Float";
				case ShaderUniformType::Vec2:	return "Vec2";
				case ShaderUniformType::Vec3:	return "Vec3";
				case ShaderUniformType::Vec4:	return "Vec4";
				case ShaderUniformType::IVec2:	return "IVec2";
				case ShaderUniformType::IVec3:	return "IVec3";
				case ShaderUniformType::IVec4:	return "IVec4";
				case ShaderUniformType::Mat3:	return "Mat3";
				case ShaderUniformType::Mat4:	return "Mat4";
			}

			return "None";
		}

	private:
		std::string m_Name;
		ShaderUniformType m_Type = ShaderUniformType::None;
		uint32_t m_Size = 0;
		uint32_t m_Offset = 0;
	};

}