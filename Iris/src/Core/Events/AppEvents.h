#pragma once

#include "Events.h"

namespace Iris::Events {

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(uint32_t width, uint32_t height)
			: m_Width(width), m_Height(height) {}

		inline uint32_t GetWidth() const { return m_Width; }
		inline uint32_t GetHeight() const { return m_Height; }

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Window Resize Event: " << m_Width << '\t' << m_Height;
			return ss.str();
		}

		IR_EVENT_CLASS_TYPE(WindowResize)

	private:
		uint32_t m_Width;
		uint32_t m_Height;

	};

	class WindowMinimizeEvent : public Event
	{
	public:
		WindowMinimizeEvent(bool minimized)
			: m_Minimized(minimized) {}

		bool IsMinimized() const { return m_Minimized; }

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Window Minimize Event!";
			return ss.str();
		}

		IR_EVENT_CLASS_TYPE(WindowMinimize)

	private:
		bool m_Minimized = false;

	};

	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Window Close Event!";
			return ss.str();
		}

		IR_EVENT_CLASS_TYPE(WindowClose)

	};

}