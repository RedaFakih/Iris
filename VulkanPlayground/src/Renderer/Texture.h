#pragma once

#include "Core/Base.h"
#include "Image.h"

#include <vulkan/vulkan.h>

namespace vkPlayground {

	struct TextureSpecification
	{
	};

	class Texture2D : public RefCountedObject
	{
	public:

		[[nodiscard]] static Ref<Texture2D> Create();

		// TODO: TEMP!!!
		void SetDescriptorImageInfo(const VkDescriptorImageInfo& descriptorInfo) { m_DescriptorInfo = descriptorInfo; }
		const VkDescriptorImageInfo& GetDescriptorImageInfo() const { return m_DescriptorInfo; }
	private:

		VkDescriptorImageInfo m_DescriptorInfo;
	};

}