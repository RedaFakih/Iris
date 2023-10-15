#pragma once

#include <cstdint>

namespace vkPlayground {

	struct GPUMemoryStats
	{
		uint64_t Used = 0; // TotalUsed
		uint64_t TotalAvailable = 0;
		uint64_t AllocationCount = 0;

		uint64_t BufferAllocationSize = 0;
		uint64_t BufferAllocationCount = 0;
	};

}