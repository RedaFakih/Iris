#pragma once

#include "Core/Thread.h"

namespace Iris {

	struct RenderThreadData;

	enum class ThreadingPolicy : uint8_t
	{
		None = 0,
		SingleThreaded,
		MultiThreaded // Creates a render thread
	};

	class RenderThread
	{
	public:
		RenderThread(ThreadingPolicy policy);
		~RenderThread();

		void Run();
		void Terminate();

		void Wait(ThreadState waitForState);
		void Set(ThreadState stateToSet);
		void WaitAndSet(ThreadState waitForState, ThreadState stateToSet);

		void NextFrame();
		void Kick();
		void BlockUntilRenderComplete();
		void Pump();

		bool IsRunning() const { return m_IsRunning.load(); }

		inline static bool IsCurrentThreadRT()
		{
			return s_RenderThreadID.load(std::memory_order_acquire) == std::this_thread::get_id();
		}

	private:
		Thread m_Thread;
		RenderThreadData* m_Data;

		ThreadingPolicy m_Policy = ThreadingPolicy::None;

		std::atomic<bool> m_IsRunning = false;

		std::atomic<uint32_t> m_AppThreadFrame = 0;

		inline static std::atomic<std::thread::id> s_RenderThreadID;

	};

}