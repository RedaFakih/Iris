#pragma once

#include <xhash>

namespace vkPlayground {

	class UUID
	{
	public:
		UUID();
		UUID(uint64_t id);
		UUID(const UUID& other);

		operator uint64_t() { return m_UUID; }
		operator const uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};

	class UUID32
	{
	public:
		UUID32();
		UUID32(uint32_t id);
		UUID32(const UUID32& other);

		operator uint32_t() { return m_UUID; }
		operator const uint32_t() const { return m_UUID; }
	private:
		uint32_t m_UUID;
	};

}

namespace std {

	template<>
	struct hash<vkPlayground::UUID>
	{
		std::size_t operator()(const vkPlayground::UUID& uuid) const
		{
			return uuid;
		}
	};

	template<>
	struct hash<vkPlayground::UUID32>
	{
		std::size_t operator()(const vkPlayground::UUID32& uuid) const
		{
			return hash<uint32_t>{}((uint32_t)uuid);
		}
	};

}