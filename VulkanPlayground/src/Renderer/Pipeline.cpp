#include "Pipeline.h"

#include "Renderer.h"
#include "Renderer/Core/RendererContext.h"

namespace vkPlayground {

	namespace Utils {

		static VkPrimitiveTopology GetVulkanTopologyFromTopology(PrimitiveTopology topology)
		{
			switch (topology)
			{
				case PrimitiveTopology::Triangles:			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				case PrimitiveTopology::TriangleStrip:      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
				case PrimitiveTopology::TriangleFan:		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
				case PrimitiveTopology::Lines:				return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
				case PrimitiveTopology::LineStrip:			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
				case PrimitiveTopology::Points:				return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			}

			PG_ASSERT(false, "Unreachable!");
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}

		static VkCompareOp GetVulkanCompareOpFromDepthComarator(DepthCompareOperator comparator)
		{
			switch (comparator)
			{
				case DepthCompareOperator::Never:			return VK_COMPARE_OP_NEVER;
				case DepthCompareOperator::NotEqual:		return VK_COMPARE_OP_NOT_EQUAL;
				case DepthCompareOperator::Less:			return VK_COMPARE_OP_LESS;
				case DepthCompareOperator::LessOrEqual:		return VK_COMPARE_OP_LESS_OR_EQUAL;
				case DepthCompareOperator::Greater:			return VK_COMPARE_OP_GREATER;
				case DepthCompareOperator::GreaterOrEqual:	return VK_COMPARE_OP_GREATER_OR_EQUAL;
				case DepthCompareOperator::Equal:			return VK_COMPARE_OP_EQUAL;
				case DepthCompareOperator::Always:			return VK_COMPARE_OP_ALWAYS;
			}

			PG_ASSERT(false, "Unreachable!");
			return VK_COMPARE_OP_MAX_ENUM;
		}

		static VkFormat GetVulkanFormatFromShaderDataType(ShaderDataType type)
		{
			switch (type)
			{
				case ShaderDataType::Float:		return VK_FORMAT_R32_SFLOAT;
				case ShaderDataType::Float2:	return VK_FORMAT_R32G32_SFLOAT;
				case ShaderDataType::Float3:	return VK_FORMAT_R32G32B32_SFLOAT;
				case ShaderDataType::Float4:	return VK_FORMAT_R32G32B32A32_SFLOAT;
				case ShaderDataType::Int:		return VK_FORMAT_R32_SINT;
				case ShaderDataType::Int2:		return VK_FORMAT_R32G32_SINT;
				case ShaderDataType::Int3:		return VK_FORMAT_R32G32B32_SINT;
				case ShaderDataType::Int4:		return VK_FORMAT_R32G32B32A32_SINT;
			}

			PG_ASSERT(false, "Unreachable!");
			return VK_FORMAT_UNDEFINED;
		}

	}

	Ref<Pipeline> Pipeline::Create(const PipelineSpecification& spec)
	{
		return CreateRef<Pipeline>(spec);
	}

	Pipeline::Pipeline(const PipelineSpecification& spec)
		: m_Specification(spec)
	{
		PG_ASSERT(spec.Shader, "Shader is not provided!");
		// PG_ASSERT(spec.TargetFramebuffer, "Framebuffer is not provided!");
		Invalidate();
		Renderer::RegisterShaderDependency(spec.Shader, this);
	}

	Pipeline::~Pipeline()
	{
		Renderer::SubmitReseourceFree([pipeline = m_VulkanPipeline, layout = m_PipelineLayout]()
		{
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, layout , nullptr);
		});
	}

	void Pipeline::Invalidate()
	{
		VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
		PG_ASSERT(m_Specification.Shader, "Shader can not be nullptr!");

		Ref<Shader> shader = m_Specification.Shader;

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = shader->GetAllDescriptorSetLayout();
		// TODO: PushConstantRanges from shader



		// Setting up the pipeline with the VkPipelineVertexInputStateCreateInfo struct to accept the vertex data that will be 
		// passed to the vertex buffer

		// NOTE: This is a vector since we will want in the future to support Instancing and that will require an instance laytou to be submitted
		std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;
		VkVertexInputBindingDescription vertexInputBinding = {
			.binding = 0,
			.stride = m_Specification.VertexLayout.GetStride(),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		vertexInputBindingDescriptions.push_back(vertexInputBinding);

		// Description for shader input attributes and their memory layouts
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributesDescriptions(m_Specification.VertexLayout.GetElementCount());

		// Binding for outerloop and location for inner loop
		// Binding here is for each layout... For example we could have another layout for instance data and its attributes would
		// be at binding 1 location 0, 1, 2... and so on.
		uint32_t binding = 0;
		uint32_t location = 0;
		std::array<VertexInputLayout, 1> pipelineLayouts = { m_Specification.VertexLayout }; // NOTE: 1 since we for now only have one layout (No instancing...)
		for (const auto& layout : pipelineLayouts)
		{
			for (const auto& element : layout)
			{
				vertexInputAttributesDescriptions[location].binding = binding;
				vertexInputAttributesDescriptions[location].location = location;
				vertexInputAttributesDescriptions[location].format = Utils::GetVulkanFormatFromShaderDataType(element.Type);
				vertexInputAttributesDescriptions[location].offset = element.Offset;

				location++;
			}

			binding++;
		}

		VkPipelineVertexInputStateCreateInfo vertexInputState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = (uint32_t)vertexInputBindingDescriptions.size(),
			.pVertexBindingDescriptions = vertexInputBindingDescriptions.data(),
			.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributesDescriptions.size(),
			.pVertexAttributeDescriptions = vertexInputAttributesDescriptions.data()
		};

		// Input assembly state describes how primitives are assembled
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = Utils::GetVulkanTopologyFromTopology(m_Specification.Topology),
			.primitiveRestartEnable = VK_FALSE
		};

		// VkViewport viewport = {
		// 	.x = 0.0f,
		// 	.y = 0.0f,
		// 	.width = (float)m_Window->GetWidth(),
		// 	.height = (float)m_Window->GetHeight(),
		// 	.minDepth = 0.0f,
		// 	.maxDepth = 1.0f
		// };
		
		// VkRect2D scissor = {
		// 	.offset = {.x = 0, .y = 0 },
		// 	.extent = {.width = 0, .height = 0 }
		// };

		// viewport state sets the number of viewport and scissor used in this pipeline however it is overidden by the dynamic state
		VkPipelineViewportStateCreateInfo viewportStateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			// .pViewports = &viewport, // If we want to statically bake the viewport state into the pipeline
			.scissorCount = 1,
			// .pScissors = &scissor // If we want to statically bake the scissor rectangle into the pipeline
		};

		// Enable dynamic states
		// Most states are baked into the pipeline, but there are still a few dynamic states that can be changed within a command buffer
		// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer.
		// For this example we will set the viewport and scissor using dynamic states
		// NOTE: If we want to bake the `viewport` and `scissor` states statically into the pipeline: Check in the above struct the comments
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		if (IsDynamicLineWidth())
			dynamicStateEnables.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);

		VkPipelineDynamicStateCreateInfo dynamicState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = (uint32_t)dynamicStateEnables.size(),
			.pDynamicStates = dynamicStateEnables.data()
		};

		VkPipelineRasterizationStateCreateInfo rasterizationState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = m_Specification.WireFrame ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
			// .frontFace = VK_FRONT_FACE_CLOCKWISE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // TODO: Comeback to this since we dont want it to be like this
			.depthBiasEnable = VK_FALSE,
			.lineWidth = m_Specification.LineWidth // Dynamic
		};
		rasterizationState.cullMode = m_Specification.BackFaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE; // Causes an error in aggregate init lol

		VkPipelineMultisampleStateCreateInfo multiSampleState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = m_Specification.DepthTest ? VK_TRUE : VK_FALSE,
			.depthWriteEnable = m_Specification.DepthWrite ? VK_TRUE : VK_FALSE,
			.depthCompareOp = Utils::GetVulkanCompareOpFromDepthComarator(m_Specification.DepthOperator),
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {
				.failOp = VK_STENCIL_OP_KEEP,
				.passOp = VK_STENCIL_OP_KEEP,
				.compareOp = VK_COMPARE_OP_ALWAYS
			},
			.back = depthStencilState.front
		};

		// Color blend state describes how blend factors are calculated (if used)
		// We need one blend attachment state per color attachment (even if blending is not used)
		// TODO: 1 since we only have one color attachment on the swapchain, Make it work nicely with the framebuffer attachments...
		size_t colorAttachmentCount = 1;
		std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(colorAttachmentCount);
		bool swapChainTarget = true; // TODO: Get from frambuffer
		if (swapChainTarget)
		{
			blendAttachmentStates[0] = {
				.blendEnable = VK_TRUE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = 0xf  // VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			};
		}
		else
		{
			// TODO: Work with the framebuffer color attachments...
		}

		VkPipelineColorBlendStateCreateInfo colorBlendState = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.attachmentCount = (uint32_t)blendAttachmentStates.size(),
			.pAttachments = blendAttachmentStates.data()
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
			.pSetLayouts = descriptorSetLayouts.data(),
			.pushConstantRangeCount = 0, // TODO: PushConstantRnages size from shader
			.pPushConstantRanges = nullptr, // TODO: PushConstantRanges data from shader
		};

		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));

		const auto& shaderStages = m_Specification.Shader->GetPipelineShaderStageCreateInfos();

		VkGraphicsPipelineCreateInfo pipelineInfo = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = &vertexInputState,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportStateInfo,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multiSampleState,
			.pDepthStencilState = &depthStencilState,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = m_PipelineLayout,
			// .renderPass =  framebuffer->GetRenderPass() // TODO: Get this renderpass from the framebuffer since it sets the correct renderpass if it is a swapchain target or not
			.renderPass = m_Specification.TemporaryPipelineSpecData.TemporaryRenderPass
		};

		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_VulkanPipeline));
		VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName, m_VulkanPipeline);
	}

}