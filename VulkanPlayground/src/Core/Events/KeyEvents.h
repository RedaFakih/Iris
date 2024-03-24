#pragma once

#include "Events.h"

namespace vkPlayground::Events {

	class KeyEvent : public Event
	{
	public:
		virtual ~KeyEvent() = default;

		[[nodiscard]] inline KeyCode GetKeyCode() const { return m_KeyCode; }

	protected:
		KeyEvent(KeyCode keycode)
			: m_KeyCode(keycode) {}

		KeyCode m_KeyCode;

	};

	class KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(KeyCode keycode, int repeatCount)
			: KeyEvent(keycode), m_RepeatCount(repeatCount) {}

		inline uint32_t GetRepeatCount() const { return m_RepeatCount; }

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Key Pressed Event: " << m_KeyCode << " (" << m_RepeatCount << "repeats)";
			return ss.str();
		}

		VKPG_EVENT_CLASS_TYPE(KeyPressed)

	private:
		uint32_t m_RepeatCount;

	};

	class KeyReleasedEvent : public KeyEvent
	{
	public:
		KeyReleasedEvent(KeyCode keycode)
			: KeyEvent(keycode) {}

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Key Released Event: " << m_KeyCode;
			return ss.str();
		}

		VKPG_EVENT_CLASS_TYPE(KeyReleased)

	};

	class KeyTypedEvent : public KeyEvent
	{
	public:
		KeyTypedEvent(KeyCode keycode)
			: KeyEvent(keycode) {}

		std::string toString() const override
		{
			std::stringstream ss;
			ss << "Key Typed Event: " << m_KeyCode;
			return ss.str();
		}

		VKPG_EVENT_CLASS_TYPE(KeyTyped)

	};

}