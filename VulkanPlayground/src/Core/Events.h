#pragma once

#include <sstream>
#include <string>

#define PG_STRINGIFY_MACRO(x) #x

namespace vkPlayground::Events {

	enum class EventType
	{
		None = 0,
		AppTick, AppUpdate, AppRender,
		WindowResize, WindowMinimize, WindowMaximize, WindowClose, WindowPathDrop,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	#define EVENT_CLASS_TYPE(type) static EventType getStaticType() { return EventType::type; }\
								virtual EventType GetEventType() const override { return getStaticType(); }\
								virtual const char* GetName() const override { return PG_STRINGIFY_MACRO(type); }

	class Event
	{
	public:
		virtual ~Event() = default;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual std::string toString() const { return GetName(); }

		bool Handled = false;

	};

	/*
	 * This switched from using std::function to using just normal function pointers which are deduced by the compiler in the
	 * dispatch function in the F template argument.
	 * The switch is to reduce potential heap allocations done by std::function!
	 */

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& e)
			: m_Event(e) {}

		template<typename T, typename Func> // Func is to be deduced by compiler, and it will be a function
		bool Dispatch(const Func& func)
		{
			if (m_Event.GetEventType() == T::getStaticType() && !m_Event.Handled)
			{
				m_Event.Handled = func(*(T*)&m_Event);

				return true;
			}

			return false;
		}

	private:
		Event& m_Event;

	};

	inline std::ostream& operator<<(std::ostream& stream, const Event& e)
	{
		return stream << e.toString();
	}

}