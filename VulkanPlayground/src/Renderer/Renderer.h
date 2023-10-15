#pragma once

#include "Core/Application.h"
#include "Renderer/Core/RendererContext.h"
#include "Renderer/Core/RenderCommandQueue.h"
#include "RendererConfiguration.h"

namespace vkPlayground {

	class Renderer
	{
	public:
		using RenderCommandFn = void(*)(void*);

		static Ref<RendererContext> GetContext() { return Application::Get().GetWindow().GetRendererContext(); }

		static void Init();
		static void Shutdown();

		// template<typename FuncT>
		// static void Submit(FuncT&& func)

		template<typename FuncT>
		static void SubmitReseourceFree(FuncT&& func)
		{
			auto renderCmd = [](void* ptr)
			{
				FuncT* pFunc = (FuncT*)ptr;
				(*pFunc)();

				// NOTE: Instead of destroying we could try to ensure all items are trivially destructible
				// however some items may contains std::string (uniforms) <- :TODO
				// static_assert(std::is_trivially_destructible_v<FuncT>, "FuncT must be trivially destructible");
				pFunc->~FuncT();
			};

			// TODO: Should be done on the render thread by wrapping it in an another Renderer::Submit(...);
			const uint32_t index = Renderer::GetCurrentFrameIndex();
			void* storageBuffer = Renderer::GetRendererResourceReleaseQueue(index).Allocate(renderCmd, sizeof(func));
			new(storageBuffer) FuncT(std::forward<FuncT>((FuncT&&)func));
		}

		static RenderCommandQueue& GetRendererResourceReleaseQueue(uint32_t index);

		static uint32_t GetCurrentFrameIndex();

		static RendererConfiguration& GetConfig();
		static void SetConfig(const RendererConfiguration& config);

	private:

	};

}