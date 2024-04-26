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
			DeleteCriticalSection(&m_Data->CriticalSection);
			m_Thread.Join();
		}

		s_RenderThreadID = std::thread::id();
	}

	void RenderThread::Run()
	{
		m_IsRunning = true;
		if (m_Policy == ThreadingPolicy::MultiThreaded)
			m_Thread.Dispatch(Renderer::RenderThreadFunc, this);

		s_RenderThreadID = m_Thread.GetID();
	}

	void RenderThread::Terminate()
	{
		m_IsRunning = false;
		Pump();

		if (m_Policy == ThreadingPolicy::MultiThreaded)
			m_Thread.Join();

		s_RenderThreadID = std::thread::id();
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

	bool RenderThread::IsCurrentThreadRT()
	{
		return s_RenderThreadID == std::this_thread::get_id();
	}

}