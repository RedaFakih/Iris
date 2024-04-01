#include "vkPch.h"
#include "VulkanAllocator.h"

#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/Vulkan.h"
#include "Utils/StringUtils.h"

// NOTE: Enable/Disable logging messages about the allocated chunks and total allocated memory since beginning...
#define LOG_MEMORY_USAGE_MESSAGES 1

namespace vkPlayground {

	struct AllocatorStaticData
	{
		VmaAllocator Allocator;
		uint64_t TotalAllocatedBytes = 0;

		uint64_t MemoryUsage = 0; // All GPU heaps
	};

	static AllocatorStaticData* s_Data;

	enum class AllocationType : uint8_t
	{
		None = 0,
		Buffer,
		Image
	};

	struct AllocationInfo
	{
		uint64_t AllocatedSize = 0;
		AllocationType Type = AllocationType::None;
	};

	static std::map<VmaAllocation, AllocationInfo> s_AllocationMap;

	VulkanAllocator::VulkanAllocator(std::string_view name)
		: m_Name(name)
	{
	}

	VulkanAllocator::~VulkanAllocator()
	{
	}

	void VulkanAllocator::Init()
	{
		s_Data = new AllocatorStaticData();

		VmaAllocatorCreateInfo createInfo = {
			.physicalDevice = RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetVulkanPhysicalDevice(),
			.device = RendererContext::GetCurrentDevice()->GetVulkanDevice(),
			.instance = RendererContext::GetInstance(),
			.vulkanApiVersion = VK_API_VERSION_1_3
		};

		VK_CHECK_RESULT(vmaCreateAllocator(&createInfo, &s_Data->Allocator));
	}

	void VulkanAllocator::Shutdown()
	{
		vmaDestroyAllocator(s_Data->Allocator);

		delete s_Data;
		s_Data = nullptr;
	}

	VmaAllocation VulkanAllocator::AllocateBuffer(const VkBufferCreateInfo* bufferCreateInfo, VmaMemoryUsage usage, VkBuffer* buffer, VmaAllocationInfo* allocationInfo)
	{
		VKPG_ASSERT(bufferCreateInfo->size > 0);

		VmaAllocationCreateInfo allocationCreateInfo = { .usage = usage };

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		vmaCreateBuffer(s_Data->Allocator, bufferCreateInfo, &allocationCreateInfo, buffer, &allocation, &allocInfo);
		if (allocation == nullptr)
		{
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "Failed to allocate GPU buffer!");
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tRequested Size: {}", Utils::BytesToString(bufferCreateInfo->size));
			GPUMemoryStats stats = GetStats();
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tGPU memory usage: {} / {}", Utils::BytesToString(stats.Used), Utils::BytesToString(stats.TotalAvailable));
		}

#if LOG_MEMORY_USAGE_MESSAGES
		VKPG_CORE_TRACE_TAG("VulkanAllocator", "{}: Allocating buffer with size: {}", m_Name, Utils::BytesToString(allocInfo.size));
#endif

		s_AllocationMap[allocation] = {
			.AllocatedSize = allocInfo.size,
			.Type = AllocationType::Buffer
		};

		s_Data->TotalAllocatedBytes += allocInfo.size;
		s_Data->MemoryUsage += allocInfo.size;

#if LOG_MEMORY_USAGE_MESSAGES
		VKPG_CORE_INFO_TAG("VulkanAllocator", "{}: Total allocated: {}, Current Memory Usage: {}", m_Name, Utils::BytesToString(s_Data->TotalAllocatedBytes), Utils::BytesToString(s_Data->MemoryUsage));
#endif

		if (allocationInfo != nullptr)
			*allocationInfo = allocInfo;

		return allocation;
	}

	void VulkanAllocator::DestroyBuffer(VmaAllocation allocation, VkBuffer buffer)
	{
		VKPG_ASSERT(buffer);
		VKPG_ASSERT(allocation);

		vmaDestroyBuffer(s_Data->Allocator, buffer, allocation);

		auto it = s_AllocationMap.find(allocation);
		if (it != s_AllocationMap.end())
		{
			s_Data->MemoryUsage -= it->second.AllocatedSize;
			s_AllocationMap.erase(it);
		}
		else
		{
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "{}: Could not find memory allocation: {}", m_Name, (void*)allocation);
		}
	}

	VmaAllocation VulkanAllocator::AllocateImage(const VkImageCreateInfo* imageCreateInfo, VmaMemoryUsage usage, VkImage* image, VkDeviceSize* allocatedSize)
	{
		VmaAllocationCreateInfo allocationCreateInfo = { .usage = usage };

		VmaAllocationInfo allocInfo;
		VmaAllocation allocation;
		vmaCreateImage(s_Data->Allocator, imageCreateInfo, &allocationCreateInfo, image, &allocation, &allocInfo);
		if (allocation == nullptr)
		{
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "Failed to allocate GPU image!");
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tRequested Size: {}x{}x{}", imageCreateInfo->extent.width, imageCreateInfo->extent.height, imageCreateInfo->extent.depth);
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tMips: {}", imageCreateInfo->mipLevels);
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tLayers: {}", imageCreateInfo->arrayLayers);
			GPUMemoryStats stats = GetStats();
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "\tGPU memory usage: {} / {}", Utils::BytesToString(stats.Used), Utils::BytesToString(stats.TotalAvailable));
		}

		if (allocatedSize)
			*allocatedSize = allocInfo.size;
#if LOG_MEMORY_USAGE_MESSAGES
		VKPG_CORE_TRACE_TAG("VulkanAllocator", "{}: Allocating image with size: {}", m_Name, Utils::BytesToString(allocInfo.size));
#endif

		s_AllocationMap[allocation] = {
			.AllocatedSize = allocInfo.size,
			.Type = AllocationType::Image
		};

		s_Data->TotalAllocatedBytes += allocInfo.size;
		s_Data->MemoryUsage += allocInfo.size;

#if LOG_MEMORY_USAGE_MESSAGES
		VKPG_CORE_INFO_TAG("VulkanAllocator", "{}: Total allocated: {}, Current Memory Usage: {}", m_Name, Utils::BytesToString(s_Data->TotalAllocatedBytes), Utils::BytesToString(s_Data->MemoryUsage));
#endif

		return allocation;
	}

	void VulkanAllocator::DestroyImage(VmaAllocation allocation, VkImage image)
	{
		VKPG_ASSERT(image);
		VKPG_ASSERT(allocation);

		vmaDestroyImage(s_Data->Allocator, image, allocation);

		auto it = s_AllocationMap.find(allocation);
		if (it != s_AllocationMap.end())
		{
			s_Data->MemoryUsage -= it->second.AllocatedSize;
			s_AllocationMap.erase(it);
		}
		else
		{
			VKPG_CORE_ERROR_TAG("VulkanAllocator", "{}: Could not find memory allocation: {}", m_Name, (void*)allocation);
		}
	}

	void VulkanAllocator::UnmapMemory(VmaAllocation allocation)
	{
		vmaUnmapMemory(s_Data->Allocator, allocation);
	}

	VmaAllocationInfo VulkanAllocator::GetAllocationInfo(VmaAllocation allocation) const
	{
		VmaAllocationInfo result;
		vmaGetAllocationInfo(s_Data->Allocator, allocation, &result);

		return result;
	}

	VmaAllocator& VulkanAllocator::GetVmaAllocator()
	{
		return s_Data->Allocator;
	}

	GPUMemoryStats VulkanAllocator::GetStats()
	{
		GPUMemoryStats stats;

		const auto& memoryProps = RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetPhysicalDeviceMemoryProps();
		std::vector<VmaBudget> budgets(memoryProps.memoryHeapCount);
		vmaGetHeapBudgets(s_Data->Allocator, budgets.data());

		uint64_t budget = 0;
		for (VmaBudget& b : budgets)
			budget += b.budget;

		for (const auto& [alloc, info] : s_AllocationMap)
		{
			stats.BufferAllocationCount++;
			stats.BufferAllocationSize += info.AllocatedSize;
		}

		stats.AllocationCount = s_AllocationMap.size();
		stats.Used = s_Data->MemoryUsage;
		stats.TotalAvailable = budget;

		return stats;
	}

	void VulkanAllocator::DumpStats()
	{
		const auto& memoryProps = RendererContext::GetCurrentDevice()->GetPhysicalDevice()->GetPhysicalDeviceMemoryProps();
		std::vector<VmaBudget> budgets(memoryProps.memoryHeapCount);
		vmaGetHeapBudgets(s_Data->Allocator, budgets.data());

		VKPG_CORE_WARN_TAG("VulkanAllocator", "-----------------------------------");
		for (VmaBudget& b : budgets)
		{
			VKPG_CORE_WARN_TAG("VulkanAllocator", "VmaBudget.statistics.allocationBytes = {0}", Utils::BytesToString(b.statistics.allocationBytes));
			VKPG_CORE_WARN_TAG("VulkanAllocator", "VmaBudget.statistics.blockBytes = {0}", Utils::BytesToString(b.statistics.blockBytes));
			VKPG_CORE_WARN_TAG("VulkanAllocator", "VmaBudget.usage = {0}", Utils::BytesToString(b.usage));
			VKPG_CORE_WARN_TAG("VulkanAllocator", "VmaBudget.budget = {0}", Utils::BytesToString(b.budget));
		}
		VKPG_CORE_WARN_TAG("VulkanAllocator", "-----------------------------------");
	}

}