#pragma once

#include <string_view>

namespace vkPlayground {

	struct Hash
	{
		static constexpr uint32_t GenerateFNVHash(std::string_view str)
		{
			constexpr uint32_t FNV_PRIME = 16777619u;
			constexpr uint32_t OFFSET_BASIS = 2166136261u;

			const size_t length = str.length();
			const char* data = str.data();

			uint32_t hash = OFFSET_BASIS;
			for (uint32_t i = 0; i < length; i++)
			{
				hash ^= *data++;
				hash *= FNV_PRIME;
			}

			hash ^= '\0';
			hash *= FNV_PRIME;

			return hash;
		}
	};

}