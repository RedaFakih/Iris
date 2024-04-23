#include "IrisPCH.h"
#include "Thread.h"

namespace Iris {

	/*
	 * Currently this only woks on windows since we use the win32 api to set the thread name
	 */

	Thread::Thread(const std::string& name)
		: m_Name(name)
	{
	}

	void Thread::Join()
	{
		if (m_Thread.joinable())
			m_Thread.join();
	}

	void Thread::SetName(std::string_view name)
	{
		HANDLE threadHandle = m_Thread.native_handle();

		std::wstring threadName(name.begin(), name.end());
		SetThreadDescription(threadHandle, threadName.c_str());
		SetThreadAffinityMask(threadHandle, 8);
	}

	std::thread::id Thread::GetID() const
	{
		return std::thread::id();
	}

}