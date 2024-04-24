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

	enum class RenderThreadState : uint8_t
	{
		Idle = 0,
		Busy,
		Kick
	};

	class RenderThread
	{
	public:
		RenderThread(ThreadingPolicy policy);
		~RenderThread();

		void Run();
		void Terminate();

		void Wait(RenderThreadState waitForState);
		void Set(RenderThreadState stateToSet);
		void WaitAndSet(RenderThreadState waitForState, RenderThreadState stateToSet);

		void NextFrame();
		void Kick();
		void BlockUntilRenderComplete();
		void Pump();

		bool IsRunning() const { return m_IsRunning; }

		static bool IsCurrentThreadRT();

	private:
		Thread m_Thread;
		RenderThreadData* m_Data;

		ThreadingPolicy m_Policy = ThreadingPolicy::None;

		bool m_IsRunning = false;

		std::atomic<uint32_t> m_AppThreadFrame = 0;

		inline static std::thread::id s_RenderThreadID;

	};

}