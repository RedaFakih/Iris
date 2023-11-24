#include "Application.h"

#include "Renderer/Shaders/Shader.h"
#include "Renderer/Renderer.h"

// TODO: THESE INCLUDES ARE TEMPORARY
#include "Renderer/Core/Vulkan.h" // TODO: TEMP! No vulkan calls should be here
#include "Renderer/Pipeline.h"
#include "Renderer/VertexBuffer.h"
#include "Renderer/IndexBuffer.h"
#include "Renderer/UniformBufferSet.h"
#include "Renderer/RenderPass.h"

#include <glfw/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// TODO: TEMP!
#include <stb_image/stb_image.h>

namespace vkPlayground {
	
	// TODO: REMOVE
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Color;
		glm::vec2 TexCoord;
	};

	struct UniformBufferData
	{
		glm::mat4 Model;
		glm::mat4 View;
		glm::mat4 Projection;
	};

	// TODO: This should NOT BE HERE
	static std::vector<Vertex> s_Vertices;
	static std::vector<uint32_t> s_Indices;
	static VkImage s_Image;
	static VmaAllocation s_ImageAllocation;
	static VkImageView s_ImageView;
	static VkSampler s_Sampler;
	static Ref<Texture2D> s_Texture;

	// TODO: REMOVE
	static void CreateStuff(Window* window)
	{
		// TODO: REMOVE
		// Interleaved vertex attributes
		s_Vertices = {
#if 0
			{ {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
#else
			{ {-1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			{ { 1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			{ { 1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			{ {-1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
#endif
		};

		s_Indices = { 0, 3, 2, 2, 1, 0 }; // Since the default front face in our pipeline is CLOCKWISE
	}

	// TODO: REMOVE!
	static void DeleteStuff()
	{
		// TODO: REMOVE!
		Renderer::SubmitReseourceFree([image = s_Image, imageView = s_ImageView, imageAllocation = s_ImageAllocation, sampler = s_Sampler]()
		{
			VulkanAllocator allocator("Textures");
			allocator.DestroyImage(imageAllocation, image);
			vkDestroyImageView(RendererContext::GetCurrentDevice()->GetVulkanDevice(), imageView, nullptr);
			vkDestroySampler(RendererContext::GetCurrentDevice()->GetVulkanDevice(), sampler, nullptr);
		});
	}

	// TODO: This is here just to create some the hello traingle maybe move all the pipeline initializtion and stuff to somewhere else...
	Application::Application(const ApplicationSpecification& spec)
	{
		PG_ASSERT(!s_Instance, "No more than 1 application can be created!");
		s_Instance = this;

		if (!spec.WorkingDirectory.empty())
			std::filesystem::current_path(spec.WorkingDirectory);

		WindowSpecification windowSpec = {
			.Title = spec.Name,
			.Width = spec.WindowWidth,
			.Height = spec.WindowHeight,
			.VSync = spec.VSync,
			.FullScreen = spec.FullScreen
		};
		m_Window = Window::Create(windowSpec);
		m_Window->Init();
		m_Window->SetEventCallbackFunction([this](Events::Event& e) {Application::OnEvent(e); });

		// Set some more configurations for the window. (e.g. start maximized/centered, is it resizable? and all the stuff...)

		Renderer::SetConfig(spec.RendererConfig);
		Renderer::Init();

		if (spec.StartMaximized)
			m_Window->Maximize();
		else
			m_Window->CentreWindow();

		m_Window->SetResizable(spec.Resizable);

		// TODO: REMOVE
		CreateStuff(m_Window.get());
	}

	Application::~Application()
	{
		// TODO: REMOVE!
		DeleteStuff();

		// TODO: Some other things maybe?
		m_Window->SetEventCallbackFunction([](Events::Event& e) {});

		Renderer::Shutdown();

		// NOTE: We can't set the s_Instance to nullptr here since the application will still be used in other parts of the application to
		// retrieve certain data to destroy other data...
		// s_Instance = nullptr;
	}

	void Application::Run()
	{
		// TODO: REMOVE!
		Ref<Shader> shader = Renderer::GetShadersLibrary()->Get("SimpleShader");
		Ref<UniformBufferSet> uniformBufferSet = UniformBufferSet::Create(sizeof(UniformBufferData));

		// Create texture stuff...
		{
			VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
			VulkanAllocator allocator("Textures");

			// decode and load the image file
			int imageWidth, imageHeight, imageChannels;
			stbi_uc* data = stbi_load("assets/textures/qiyana.png", &imageWidth, &imageHeight, &imageChannels, STBI_rgb_alpha);
			uint64_t imageSize = imageWidth * imageHeight * 4;

			VkBuffer stagingBuffer;
			VkBufferCreateInfo stagingBufferCI = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = imageSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};

			VmaAllocation stagingBufferAlloc = allocator.AllocateBuffer(&stagingBufferCI, VMA_MEMORY_USAGE_CPU_TO_GPU, &stagingBuffer);
			uint8_t* dstData = allocator.MapMemory<uint8_t>(stagingBufferAlloc);
			std::memcpy(dstData, data, imageSize);
			allocator.UnmapMemory(stagingBufferAlloc);

			stbi_image_free(data);

			VkImageCreateInfo imageCI = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.flags = 0,
				.imageType = VK_IMAGE_TYPE_2D, // tells vulkan the coordinate system to use in the shader for this image
				.format = VK_FORMAT_R8G8B8A8_SRGB,
				.extent = { .width = (uint32_t)imageWidth, .height = (uint32_t)imageHeight, .depth = 1u },
				.mipLevels = 1, // We will not be using mip mapping so we only have one mip which is the main image
				.arrayLayers = 1, // Texture will not be an array so we have only one layer
				.samples = VK_SAMPLE_COUNT_1_BIT, // Not a multisampled image
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Since we will transfer data from staging buffer to it
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED // Not usable by gpu and first transition will discard the texels
			};

			VkDeviceSize size;
			s_ImageAllocation = allocator.AllocateImage(&imageCI, VMA_MEMORY_USAGE_GPU_ONLY, &s_Image, &size);

			VkCommandBuffer commandBuffer = RendererContext::GetCurrentDevice()->GetCommandBuffer(true);

			// Transition image layout from IMAGE_LAYOUT_UNDEFINED -> IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			{
				// First Transition to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

				// Pipeline Barriers are used to synchronize access to resources... 
				// They ensure that a resource finished doing something before doing something else
				// 
				// *** Certain edge cases to handle with src/dstAccessMask ***
				// if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
				// {
				// 	barrier.srcAccessMask = 0;
				// 	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				//  sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				//  destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				// }
				// else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				// {
				// 	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				// 	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				// 
				// 	sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				// 	destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				// }

				VkPipelineStageFlags sourceStage, destinationStage;
				
				VkImageMemoryBarrier imageMemBarrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = s_Image,
					.subresourceRange = { 
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};

				sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

				vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);

				// Copying image data from staging buffer:

				VkBufferImageCopy copyRegion = {
					.bufferOffset = 0,
					.bufferRowLength = 0,
					.bufferImageHeight = 0,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.imageOffset = { .x = 0, .y = 0, .z = 0 },
					.imageExtent = { .width = (uint32_t)imageWidth, .height = (uint32_t)imageHeight, .depth = 1u }
				};

				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, s_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

				// Second Transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

				imageMemBarrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = s_Image,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};

				sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
			}

			RendererContext::GetCurrentDevice()->FlushCommandBuffer(commandBuffer);
			allocator.DestroyBuffer(stagingBufferAlloc, stagingBuffer);

			// Create Image view
			{
				VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();

				VkImageViewCreateInfo viewInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = s_Image,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = VK_FORMAT_R8G8B8A8_SRGB,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				};

				VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &s_ImageView));
			}

			// Create sampler for the texture that defines how the image is filtered and applies transformation for the final pixel color before it is
			// ready to be retrieved by the shader
			// Sampler do not reference an image like in old APIs, rather they are their own standalone objects and can be used for multiple types of
			// images
			{
				VkDevice device = RendererContext::GetCurrentDevice()->GetVulkanDevice();
				Ref<VulkanPhysicalDevice> physicalDevice = RendererContext::GetCurrentDevice()->GetPhysicalDevice();
				VkSamplerCreateInfo samplerCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
					.magFilter = VK_FILTER_LINEAR,
					.minFilter = VK_FILTER_LINEAR,
					// NOTE: This is related to mip mapping and we will look at it later... `TODO`
					.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
					// Repeat or clamp to border/edge?
					.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
					// NOTE: This is related to mip mapping and we will look at it later... `TODO`
					.mipLodBias = 0.0f,
					.anisotropyEnable = VK_TRUE,
					.maxAnisotropy = physicalDevice->GetPhysicalDeviceProperties().limits.maxSamplerAnisotropy,
					// NOTE: This aplies a comparison function and based on its results the texel is affected
					// Comparison operators are mainly used for shadow mapping
					.compareEnable = VK_FALSE,
					.compareOp = VK_COMPARE_OP_ALWAYS,
					// NOTE: This is related to mip mapping and we will look at it later... `TODO`
					.minLod = 0.0f,
					.maxLod = 0.0f,
					.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
					// NOTE: Use coordinate system [0, 1) in order to access texels in the texture instead of [0, textureWidth)
					.unnormalizedCoordinates = VK_FALSE,
				};

				VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &s_Sampler));
			}

			s_Texture = Texture2D::Create();
			s_Texture->SetDescriptorImageInfo({
				.sampler = s_Sampler,
				.imageView = s_ImageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			});
		}

		VertexInputLayout layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Color"    },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		// TODO: TEMPORARY
		TempPipelineSpecData tempPipelineSpecData = {
			.TemporaryRenderPass = m_Window->GetSwapChain().GetRenderPass(),
		};

		PipelineSpecification spec = {
			.DebugName = "PlaygroundPipeline",
			.Shader = shader,
			.TemporaryPipelineSpecData = tempPipelineSpecData, // TODO: TEMPORARY
			.TargetFramebuffer = nullptr,
			.VertexLayout = layout,
			.Topology = PrimitiveTopology::Triangles,
			.DepthOperator = DepthCompareOperator::Greater,
			.BackFaceCulling = true,
			.DepthTest = false,
			.DepthWrite = false,
			.WireFrame = false,
			.LineWidth = 1.0f
		};
		Ref<Pipeline> pipeline = Pipeline::Create(spec);

		RenderPassSpecification mainPassSpec = {
			.DebugName = "MainPass",
			.Pipeline = pipeline,
			.MarkerColor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)
		};
		Ref<RenderPass> mainPass = RenderPass::Create(mainPassSpec);
		mainPass->SetInput("TransformUniformBuffer", uniformBufferSet);
		mainPass->SetInput("u_Texture", s_Texture);
		mainPass->Bake();

		Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(s_Vertices.data(), (uint32_t)(sizeof(Vertex) * s_Vertices.size()));
		Ref<IndexBuffer> indexBuffer = IndexBuffer::Create(s_Indices.data(), (uint32_t)(sizeof(uint32_t) * s_Indices.size()));

		// NOTE: We can release the shader modules after the pipeline have been created
		shader->Release();

		while (m_Running)
		{
			static uint64_t frameCounter = 0;

			ProcessEvents();

			if (!m_Minimized)
			{
				m_Window->GetSwapChain().BeginFrame();

				// Update uniform buffers (Begin Scene stuff)
				UniformBufferData dataUB = {
					.Model = glm::rotate(glm::mat4(1.0f), GetTime() * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
					.View = glm::lookAt(glm::vec3(2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
					.Projection = glm::perspective(glm::radians(45.0f), m_Window->GetSwapChain().GetWidth() / (float)m_Window->GetSwapChain().GetHeight(), 0.1f, 10.0f)
				};

				dataUB.Projection[1][1] *= -1; // Flip the Y axis

				uniformBufferSet->Get()->SetData(&dataUB, sizeof(UniformBufferData));

				VkCommandBuffer commandBuffer = m_Window->GetSwapChain().GetCurrentDrawCommandBuffer();

				VkCommandBufferBeginInfo commandBufferBeginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

				VkRenderPassBeginInfo renderPassBeginInfo = {
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = m_Window->GetSwapChain().GetRenderPass(),
					.framebuffer = m_Window->GetSwapChain().GetCurrentFramebuffer(),
					.renderArea = {
						.offset = { .x = 0, .y = 0 },
						.extent = { .width = m_Window->GetSwapChain().GetWidth(), .height = m_Window->GetSwapChain().GetHeight() }
					},
					.clearValueCount = 1,
					.pClearValues = [](auto&& temp) { return &temp; }(VkClearValue{ .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } } })
				};

				vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					VkViewport viewport = {
						.x = 0.0f,
						.y = 0.0f,
						.width = (float)m_Window->GetSwapChain().GetWidth(),
						.height = (float)m_Window->GetSwapChain().GetHeight(),
						.minDepth = 0.0f,
						.maxDepth = 1.0f
					};
					vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

					VkRect2D scissor = {
						.offset = { 0, 0 },
						.extent = {.width = m_Window->GetSwapChain().GetWidth(), .height = m_Window->GetSwapChain().GetHeight() }
					};
					vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

					vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVulkanPipeline());

					if (pipeline->IsDynamicLineWidth())
						vkCmdSetLineWidth(commandBuffer, pipeline->GetSpecification().LineWidth);

					VkBuffer vertexVulkanBuffer = vertexBuffer->GetVulkanBuffer();
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexVulkanBuffer, [](VkDeviceSize&& s) { return &s; }(0));
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetVulkanBuffer(), 0, VK_INDEX_TYPE_UINT32);

					mainPass->Prepare();
					if (mainPass->HasDescriptorSets())
					{
						const auto& descriptorSets = mainPass->GetDescriptorSets(m_CurrentFrameIndex);

						vkCmdBindDescriptorSets(
							commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipeline->GetVulkanPipelineLayout(),
							mainPass->GetFirstSetIndex(),
							(uint32_t)descriptorSets.size(),
							descriptorSets.data(),
							0,
							nullptr);
					}

					vkCmdDrawIndexed(commandBuffer, (uint32_t)s_Indices.size(), 1, 0, 0, 0);
				}
				vkCmdEndRenderPass(commandBuffer);

				vkEndCommandBuffer(commandBuffer);

				m_Window->SwapBuffers();

				m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % Renderer::GetConfig().FramesInFlight;
			}

			float time = GetTime();
			m_FrameTime = time - m_LastFrameTime;
			m_TimeStep = glm::min<float>(m_FrameTime, 0.0333f);
			m_LastFrameTime = time;

			++frameCounter;
		}
	}

	void Application::ProcessEvents()
	{
		m_Window->ProcessEvents();

		// NOTE: In case we were to run the events on a different thread
		std::scoped_lock<std::mutex> eventLock(m_EventQueueMutex);

		while (m_EventQueue.size())
		{
			auto& event = m_EventQueue.front();
			event();
			m_EventQueue.pop();
		}
	}

	void Application::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);

		dispatcher.Dispatch<Events::WindowResizeEvent>([this](Events::WindowResizeEvent& event) { return OnWindowResize(event); });
		dispatcher.Dispatch<Events::WindowCloseEvent>([this](Events::WindowCloseEvent& event) { return OnWindowClose(event); });
		dispatcher.Dispatch<Events::WindowMinimizeEvent>([this](Events::WindowMinimizeEvent& event) { return OnWindowMinimize(event); });
	}

	bool Application::OnWindowResize(Events::WindowResizeEvent& e)
	{
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			// m_Minimized = true; No need to set this here since there is no way the window can be minimized to (0, 0) (otherwise would crash) unless it was `iconified` and that event is handled separately
			return false;
		}

		m_Window->GetSwapChain().OnResize(width, height);

		return false;
	}

	bool Application::OnWindowClose(Events::WindowCloseEvent& e)
	{
		m_Running = false;

		return false;
	}

	bool Application::OnWindowMinimize(Events::WindowMinimizeEvent& e)
	{
		m_Minimized = e.IsMinimized();

		return false;
	}

	float Application::GetTime() const
	{
		return static_cast<float>(glfwGetTime());
	}

}