#pragma once

#include "Core/Base.h"

#include <glm/glm.hpp>

namespace vkPlayground {

	struct RenderPassSpecification
	{
		std::string DebugName;
		glm::vec4 MarkerColor;
		// TODO: Add a target framebuffer
	};

	class RenderPass : public RefCountedObject
	{
	public:
		RenderPass(const RenderPassSpecification& spec);
		~RenderPass();

		[[nodiscard]] static Ref<RenderPass> Create(const RenderPassSpecification& spec);

		RenderPassSpecification& GetSpec() { return m_Specification; }
		const RenderPassSpecification& GetSpec() const { return m_Specification; }

	private:
		RenderPassSpecification m_Specification;

	};

}