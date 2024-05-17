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

	class WindowTitleBarHitTestEvent : public Event
	{
	public:
		WindowTitleBarHitTestEvent(int x, int y, int& hit)
			: m_X(x), m_Y(y), m_Hit(hit) {}

		inline int GetX() const { return m_X; }
		inline int GetY() const { return m_Y; }
		inline void SetHit(bool hit) { m_Hit = static_cast<int>(hit); }

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Window Titlebar Hit Test Event! X: " << m_X << " Y: " << m_Y << " Hit: " << m_Hit;
			return ss.str();
		}

		IR_EVENT_CLASS_TYPE(WindowTitleBarHitTest)

	private:
		int m_X;
		int m_Y;
		int& m_Hit;

	};

	class TitleBarColorChangeEvent : public Event
	{
	public:
		TitleBarColorChangeEvent(uint32_t targetColor)
			: m_TargetColor(targetColor) {}

		inline uint32_t GetTargetColor() const { return m_TargetColor; }

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Window Titlebar Color Change Event! TargerColor: " << m_TargetColor;
			return ss.str();
		}

		IR_EVENT_CLASS_TYPE(TitleBarColorChange)

	private:
		uint32_t m_TargetColor;

	};

	class RenderViewportOnlyEvent : public Event
	{
		public:
			RenderViewportOnlyEvent(bool flag)
				: m_ViewportOnly(flag) {}

			inline bool GetViewportOnlyFlag() const { return m_ViewportOnly; }

			std::string toString() const override
			{
				std::stringstream ss;
				ss << "Render Viewport Only Event! Flag: " << m_ViewportOnly;
				return ss.str();
			}

			IR_EVENT_CLASS_TYPE(RenderViewportOnly)

	private:
		bool m_ViewportOnly = false;

	};

}