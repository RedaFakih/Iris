#pragma once

#include "Core/Input/KeyCodes.h"
#include "Core/Input/MouseCodes.h"

#include <sstream>
#include <string>

#define IR_STRINGIFY_MACRO(x) #x
#define IR_SET_EVENT_FN(function) [this](auto&&... args) -> decltype(auto) { return this->function(std::forward<decltype(args)>(args)...); }

namespace Iris::Events {

	enum class EventType
	{
		None = 0,
		/* TODO: AppTick,  AppUpdate, AppRender, */ TitleBarColorChange, RenderViewportOnly, AssetCreatedNotification,
		WindowResize, WindowMinimize, WindowClose, /* TODO: WindowPathDrop, */ WindowTitleBarHitTest,
		KeyPressed, KeyReleased, KeyTyped,
		MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
	};

	#define IR_EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::type; }\
								        virtual EventType GetEventType() const override { return GetStaticType(); }\
								        virtual const char* GetName() const override { return IR_STRINGIFY_MACRO(type); }

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
			requires requires { { T::GetStaticType() } -> std::same_as<EventType>; }
		bool Dispatch(Func&& func)
		{
			if (m_Event.GetEventType() == T::GetStaticType() && !m_Event.Handled)
			{
				m_Event.Handled = std::forward<Func>(func)(*(T*)&m_Event);

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