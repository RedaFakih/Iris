#pragma once

#include <thread>

namespace Iris {

	enum class ThreadState : uint8_t
	{
		Idle = 0,
		Busy,
		Kick,
		Joined
	};

	class Thread
	{
	public:
		Thread(const std::string& name);

		template<typename Fn, typename... Args>
		void Dispatch(Fn&& fn, Args&&... args)
		{
			m_Thread = std::thread(std::forward<Fn>(fn), std::forward<Args>(args)...);
			SetName(m_Name);
		}

		void Join();

		// Uses win32 api to set the name of the thread
		void SetName(std::string_view name);

		std::thread::id GetID() const;

	private:
		std::thread m_Thread;
		std::string m_Name;

	};

}