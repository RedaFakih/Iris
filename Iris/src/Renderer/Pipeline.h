#pragma once

#include "Framebuffer.h"
#include "Renderer/Core/Vulkan.h"
#include "Renderer/Shaders/Shader.h"
#include "VertexBuffer.h"

#include <string>

namespace Iris {

	enum class PrimitiveTopology : uint8_t
	{
		None = 0,
		Triangles,
		TriangleStrip,
		TriangleFan,
		Lines,
		LineStrip,
		Points
	};

	enum class DepthCompareOperator : uint8_t
	{
		None = 0,
		Never,
		NotEqual,
		Less,
		LessOrEqual,
		Greater,
		GreaterOrEqual,
		Equal,
		Always
	};

	// NOTE: For Instancing we need to add one more layout for the instance data
	struct PipelineSpecification
	{
		std::string DebugName;
		Ref<Shader> Shader;
		Ref<Framebuffer> TargetFramebuffer;
		VertexInputLayout VertexLayout;
		VertexInputLayout InstanceLayout;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		bool BackFaceCulling = true;
		DepthCompareOperator DepthOperator = DepthCompareOperator::GreaterOrEqual;
		bool DepthTest = true;
		bool DepthWrite = true;
		bool WireFrame = false;
		float LineWidth = 1.0f;

		// NOTE: Shader modules are usually not needed after the pipeline has been created, however we have pipelines that depend on the same shader so we cant release
		// the modules directly after the first pipeline is created
		bool ReleaseShaderModules = false;

		// NOTE: This determines whether the renderer keeps track of this dependecy. You may NOT want that since you are creating just some placeholder pipeline that
		// you do not want the renderer to keep track of.
		bool RegisterAsShaderDependency = true;
	};

	struct PipelineStatistics
	{
		uint64_t InputAssemblyVertices = 0;
		uint64_t InputAssemblyPrimitives = 0;
		uint64_t VertexShaderInvocations = 0;
		uint64_t ClippingInvocations = 0;
		uint64_t ClippingPrimitives = 0;
		uint64_t FragmentShaderInvocations = 0;
		uint64_t ComputeShaderInvocations = 0;
	};

	class Pipeline : public RefCountedObject
	{
	public:
		Pipeline(const PipelineSpecification& spec);
		~Pipeline();

		[[nodiscard]] static Ref<Pipeline> Create(const PipelineSpecification& spec);

		void Invalidate();

		PipelineSpecification& GetSpecification() { return m_Specification; }
		const PipelineSpecification& GetSpecification() const { return m_Specification; }

		Ref<Shader> GetShader() const { return m_Specification.Shader; }
		VkPipeline GetVulkanPipeline() { return m_VulkanPipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() { return m_PipelineLayout; }

		inline bool IsDynamicLineWidth() const { return m_Specification.Topology == PrimitiveTopology::Lines || m_Specification.Topology == PrimitiveTopology::LineStrip || m_Specification.WireFrame; }

	private:
		void Release();

	private:
		PipelineSpecification m_Specification;

		VkPipeline m_VulkanPipeline = nullptr;
		VkPipelineLayout m_PipelineLayout = nullptr;
	};

}