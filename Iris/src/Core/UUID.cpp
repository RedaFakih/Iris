#include "IrisPCH.h"
#include "UUID.h"

#include <random>

namespace Iris {

	static std::random_device s_RandomDevice;
	static std::mt19937_64 s_Eng(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformIntDistribution;

	static std::mt19937 s_Eng32(s_RandomDevice());
	static std::uniform_int_distribution<uint32_t> s_UniformIntDistribution32;

	UUID::UUID()
		: m_UUID(s_UniformIntDistribution(s_Eng))
	{
	}

	UUID::UUID(uint64_t id)
		: m_UUID(id)
	{
	}

	UUID::UUID(const UUID& other)
		: m_UUID(other.m_UUID)
	{
	}

	UUID32::UUID32()
		: m_UUID(s_UniformIntDistribution32(s_Eng32))
	{
	}

	UUID32::UUID32(uint32_t id)
		: m_UUID(id)
	{
	}

	UUID32::UUID32(const UUID32& other)
		: m_UUID(other.m_UUID)
	{
	}

}
