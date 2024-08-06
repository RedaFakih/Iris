#include "IrisPCH.h"
#include "Pipeline.h"

#include "Renderer.h"
#include "Renderer/Core/RendererContext.h"

namespace Iris {

	namespace Utils {

		inline constexpr VkPrimitiveTopology GetVulkanTopologyFromTopology(PrimitiveTopology topology)
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

			IR_ASSERT(false);
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}

		inline constexpr VkCompareOp GetVulkanCompareOpFromDepthComarator(DepthCompareOperator comparator)
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

			IR_ASSERT(false);
			return VK_COMPARE_OP_MAX_ENUM;
		}

		inline constexpr VkFormat GetVulkanFormatFromShaderDataType(ShaderDataType type)
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
				case ShaderDataType::UInt:		return VK_FORMAT_R32_UINT;
				case ShaderDataType::UInt2:		return VK_FORMAT_R32G32_UINT;
				case ShaderDataType::UInt3:		return VK_FORMAT_R32G32B32_UINT;
				case ShaderDataType::UInt4:		return VK_FORMAT_R32G32B32A32_UINT;
			}

			IR_ASSERT(false);
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
		IR_ASSERT(spec.Shader);
		IR_ASSERT(spec.TargetFramebuffer);

		Invalidate();

		if (spec.RegisterAsShaderDependency)
			Renderer::RegisterShaderDependency(spec.Shader, this);
	}

	Pipeline::~Pipeline()
	{
		Release();
	}

	void Pipeline::Invalidate()
	{
		Ref<Pipeline> instance = this;
		Renderer::Submit([instance]() mutable
		{
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
			IR_ASSERT(instance->m_Specification.Shader);

			// Make sure past resources are cleared in case we are reloading shaders
			instance->Release();

			// Setting up the pipeline with the VkPipelineVertexInputStateCreateInfo struct to accept the vertex data that will be 
			// passed to the vertex buffer

			std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;

			// Vertex attributes
			VkVertexInputBindingDescription vertexInputBinding = {
				.binding = 0,
				.stride = instance->m_Specification.VertexLayout.GetStride(),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			};
			vertexInputBindingDescriptions.push_back(vertexInputBinding);

			// Instance attributes
			if (instance->m_Specification.InstanceLayout.GetElementCount())
			{
				VkVertexInputBindingDescription instanceInputBinding = {
					.binding = 1,
					.stride = instance->m_Specification.InstanceLayout.GetStride(),
					.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
				};
				vertexInputBindingDescriptions.push_back(instanceInputBinding);
			}

			// Description for shader input attributes and their memory layouts
			std::vector<VkVertexInputAttributeDescription> vertexInputAttributesDescriptions(instance->m_Specification.VertexLayout.GetElementCount() + instance->m_Specification.InstanceLayout.GetElementCount());

			uint32_t binding = 0;
			uint32_t location = 0;
			std::array<VertexInputLayout, 2> pipelineLayouts = { instance->m_Specification.VertexLayout, instance->m_Specification.InstanceLayout };
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
				.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindingDescriptions.size()),
				.pVertexBindingDescriptions = vertexInputBindingDescriptions.data(),
				.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributesDescriptions.size()),
				.pVertexAttributeDescriptions = vertexInputAttributesDescriptions.data()
			};

			// Input assembly state describes how primitives are assembled
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = Utils::GetVulkanTopologyFromTopology(instance->m_Specification.Topology)
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
			if (instance->IsDynamicLineWidth())
				dynamicStateEnables.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);

			VkPipelineDynamicStateCreateInfo dynamicState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
				.pDynamicStates = dynamicStateEnables.data()
			};

			VkPipelineRasterizationStateCreateInfo rasterizationState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.polygonMode = instance->m_Specification.WireFrame ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
				.frontFace = VK_FRONT_FACE_CLOCKWISE, // default for vkPlayground
				.depthBiasEnable = VK_FALSE,
				.lineWidth = instance->m_Specification.LineWidth // Dynamic
			};
			rasterizationState.cullMode = instance->m_Specification.BackFaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE; // Causes an error in aggregate init lol

			VkPipelineMultisampleStateCreateInfo multiSampleState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.pSampleMask = nullptr,
			};

			VkPipelineDepthStencilStateCreateInfo depthStencilState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = instance->m_Specification.DepthTest ? VK_TRUE : VK_FALSE,
				.depthWriteEnable = instance->m_Specification.DepthWrite ? VK_TRUE : VK_FALSE,
				.depthCompareOp = Utils::GetVulkanCompareOpFromDepthComarator(instance->m_Specification.DepthOperator),
				.depthBoundsTestEnable = VK_FALSE,
				.stencilTestEnable = VK_FALSE,
				.front = {
					.failOp = VK_STENCIL_OP_KEEP,
					.passOp = VK_STENCIL_OP_KEEP,
					.compareOp = VK_COMPARE_OP_ALWAYS
				},
				.back = {
					.failOp = VK_STENCIL_OP_KEEP,
					.passOp = VK_STENCIL_OP_KEEP,
					.compareOp = VK_COMPARE_OP_ALWAYS
				}
			};

			// Color blend state describes how blend factors are calculated (if used)
			// We need one blend attachment state per color attachment (even if blending is not used)
			Ref<Framebuffer> framebuffer = instance->m_Specification.TargetFramebuffer;
			std::size_t colorAttachmentCount = framebuffer->GetColorAttachmentCount();
			std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(colorAttachmentCount);
			if (framebuffer->GetSpecification().SwapchainTarget)
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
				for (std::size_t i = 0; i < colorAttachmentCount; i++)
				{
					if (!framebuffer->GetSpecification().Blend)
						break;

					blendAttachmentStates[i].colorWriteMask = 0xf;

					const FramebufferTextureSpecification& attachmentSpec = framebuffer->GetSpecification().Attachments.Attachments[i];
					FramebufferBlendMode blendMode;
					if (framebuffer->GetSpecification().BlendMode == FramebufferBlendMode::None)
						blendMode = attachmentSpec.BlendMode;
					else
						blendMode = framebuffer->GetSpecification().BlendMode;

					blendAttachmentStates[i].blendEnable = attachmentSpec.Blend ? VK_TRUE : VK_FALSE;

					blendAttachmentStates[i].colorBlendOp = VK_BLEND_OP_ADD;
					blendAttachmentStates[i].alphaBlendOp = VK_BLEND_OP_ADD;
					blendAttachmentStates[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
					blendAttachmentStates[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

					switch (blendMode)
					{
						case FramebufferBlendMode::OneZero:
						{
							blendAttachmentStates[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
							blendAttachmentStates[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;

							break;
						}
						case FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha:
						{
							blendAttachmentStates[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
							blendAttachmentStates[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
							blendAttachmentStates[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
							blendAttachmentStates[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

							break;
						}
						case FramebufferBlendMode::ZeroSrcColor:
						{
							blendAttachmentStates[i].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
							blendAttachmentStates[i].dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;

							break;
						}
						default:
							IR_ASSERT(false);
					}
				}
			}

			VkPipelineColorBlendStateCreateInfo colorBlendState = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = VK_FALSE,
				.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size()),
				.pAttachments = blendAttachmentStates.data()
			};

			Ref<Shader> shader = instance->m_Specification.Shader;

			const std::vector<ShaderResources::PushConstantRange>& pushConstantRanges = shader->GetPushConstantRanges();
			std::vector<VkPushConstantRange> vulkanPushConstantRanges(pushConstantRanges.size());
			for (uint32_t i = 0; i < pushConstantRanges.size(); i++)
			{
				const ShaderResources::PushConstantRange& pushConstantRange = pushConstantRanges[i];
				VkPushConstantRange& vulkanPushConstantRange = vulkanPushConstantRanges[i];

				vulkanPushConstantRange.stageFlags = pushConstantRange.ShaderStage;
				vulkanPushConstantRange.size = pushConstantRange.Size;
				vulkanPushConstantRange.offset = pushConstantRange.Offset;
			}

			std::vector<VkDescriptorSetLayout> descriptorSetLayouts = shader->GetAllDescriptorSetLayouts();
			VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
				.pSetLayouts = descriptorSetLayouts.data(),
				.pushConstantRangeCount = static_cast<uint32_t>(vulkanPushConstantRanges.size()),
				.pPushConstantRanges = vulkanPushConstantRanges.data()
			};

			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &(instance->m_PipelineLayout)));

			const auto& shaderStages = shader->GetPipelineShaderStageCreateInfos();

			VkPipelineRenderingCreateInfo pipelineRenderingInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentCount),
				.pColorAttachmentFormats = framebuffer->GetColorAttachmentImageFormats().data(),
				.depthAttachmentFormat = framebuffer->HasDepthAttachment() ? framebuffer->GetDepthAttachmentImageFormat() : VK_FORMAT_UNDEFINED,
				.stencilAttachmentFormat = framebuffer->HasStencilComponent() ? framebuffer->GetDepthAttachmentImageFormat() : VK_FORMAT_UNDEFINED
			};

			VkGraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.pNext = &pipelineRenderingInfo;
			pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
			pipelineInfo.pStages = shaderStages.data();
			pipelineInfo.pVertexInputState = &vertexInputState;
			pipelineInfo.pInputAssemblyState = &inputAssemblyState;
			pipelineInfo.pViewportState = &viewportStateInfo;
			pipelineInfo.pRasterizationState = &rasterizationState;
			pipelineInfo.pMultisampleState = &multiSampleState;
			pipelineInfo.pDepthStencilState = &depthStencilState;
			pipelineInfo.pColorBlendState = &colorBlendState;
			pipelineInfo.pDynamicState = &dynamicState;
			pipelineInfo.layout = instance->m_PipelineLayout;
			pipelineInfo.renderPass = VK_NULL_HANDLE;

			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &(instance->m_VulkanPipeline)));
			VKUtils::SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, instance->m_Specification.DebugName, instance->m_VulkanPipeline);

			if (instance->m_Specification.ReleaseShaderModules)
				shader->ReleaseShaderModules();
		});
	}

	void Pipeline::Release()
	{
		if (m_VulkanPipeline && m_PipelineLayout)
		{
			Renderer::SubmitReseourceFree([pipeline = m_VulkanPipeline, layout = m_PipelineLayout]()
			{
				VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
				vkDestroyPipeline(device, pipeline, nullptr);
				vkDestroyPipelineLayout(device, layout, nullptr);
			});
		}
	}

}