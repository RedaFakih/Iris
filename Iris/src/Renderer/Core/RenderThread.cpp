#include "IrisPCH.h"
#include "RenderThread.h"

#include "Renderer/Renderer.h"

namespace Iris {

	struct RenderThreadData
	{
		CRITICAL_SECTION CriticalSection;
		CONDITION_VARIABLE ConditionVariable;

		ThreadState State = ThreadState::Idle;
	};

	RenderThread::RenderThread(ThreadingPolicy policy)
		: m_Thread("Render Thread"), m_Policy(policy)
	{
		m_Data = new RenderThreadData();

		if (m_Policy == ThreadingPolicy::MultiThreaded)
		{
			InitializeCriticalSection(&m_Data->CriticalSection);
			InitializeConditionVariable(&m_Data->ConditionVariable);
		}
	}

	RenderThread::~RenderThread()
	{
		if (m_Policy == ThreadingPolicy::MultiThreaded)
		{
			m_Thread.Join();
			DeleteCriticalSection(&m_Data->CriticalSection);
		}

		delete m_Data;
	}

	void RenderThread::Run()
	{
		m_IsRunning.store(true, std::memory_order_release);
		if (m_Policy == ThreadingPolicy::MultiThreaded)
		{
			m_Thread.Dispatch([this]()
			{
				s_RenderThreadID.store(std::this_thread::get_id(), std::memory_order_release);
				Renderer::RenderThreadFunc(this);
				s_RenderThreadID.store(std::thread::id(), std::memory_order_release);
			});
		}
	}

	void RenderThread::Terminate()
	{
		m_IsRunning.store(false, std::memory_order_release);
		Pump();

		if (m_Policy == ThreadingPolicy::MultiThreaded)
			m_Thread.Join();
	}

	void RenderThread::Wait(ThreadState waitForState)
	{
		if (m_Policy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		while (m_Data->State != waitForState)
		{
			// This releases the CS so that another thread can wake it
			SleepConditionVariableCS(&m_Data->ConditionVariable, &m_Data->CriticalSection, INFINITE);
		}
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void RenderThread::Set(ThreadState stateToSet)
	{
		if (m_Policy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		m_Data->State = stateToSet;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void RenderThread::WaitAndSet(ThreadState waitForState, ThreadState stateToSet)
	{
		if (m_Policy == ThreadingPolicy::SingleThreaded)
			return;

		EnterCriticalSection(&m_Data->CriticalSection);
		while (m_Data->State != waitForState)
		{
			SleepConditionVariableCS(&m_Data->ConditionVariable, &m_Data->CriticalSection, INFINITE);
		}
		m_Data->State = stateToSet;
		WakeAllConditionVariable(&m_Data->ConditionVariable);
		LeaveCriticalSection(&m_Data->CriticalSection);
	}

	void RenderThread::NextFrame()
	{
		m_AppThreadFrame++;
		Renderer::SwapQueues();
	}

	void RenderThread::Kick()
	{
		if (m_Policy == ThreadingPolicy::MultiThreaded)
		{
			Set(ThreadState::Kick);
		}
		else
		{
			Renderer::WaitAndRender(this);
		}
	}

	void RenderThread::BlockUntilRenderComplete()
	{
		if (m_Policy == ThreadingPolicy::SingleThreaded)
			return;

		Wait(ThreadState::Idle);
	}

	void RenderThread::Pump()
	{
		NextFrame();
		Kick();
		BlockUntilRenderComplete();
	}

}