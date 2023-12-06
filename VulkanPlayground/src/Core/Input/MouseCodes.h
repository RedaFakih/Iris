#pragma once

#include <ostream>

namespace vkPlayground {

	enum class MouseButton : uint8_t
	{
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Button6 = 6,
		Button7 = 7,

		ButtonLeft = Button0,
		ButtonRight = Button1,
		ButtonMiddle = Button2
	};

	inline std::ostream& operator<<(std::ostream& stream, MouseButton code)
	{
		stream << (int)code;

		return stream;
	}

}

#define AR_MOUSE_BUTTON_0        ::vkPlayground::MouseButton::Button0
#define AR_MOUSE_BUTTON_1        ::vkPlayground::MouseButton::Button1
#define AR_MOUSE_BUTTON_2        ::vkPlayground::MouseButton::Button2
#define AR_MOUSE_BUTTON_3        ::vkPlayground::MouseButton::Button3
#define AR_MOUSE_BUTTON_4        ::vkPlayground::MouseButton::Button4
#define AR_MOUSE_BUTTON_5        ::vkPlayground::MouseButton::Button5
#define AR_MOUSE_BUTTON_6        ::vkPlayground::MouseButton::Button6
#define AR_MOUSE_BUTTON_7        ::vkPlayground::MouseButton::Button7

#define AR_MOUSE_BUTTON_LEFT     ::vkPlayground::MouseButton::ButtonLeft
#define AR_MOUSE_BUTTON_RIGHT    ::vkPlayground::MouseButton::ButtonRight
#define AR_MOUSE_BUTTON_MIDDLE   ::vkPlayground::MouseButton::ButtonMiddle#pragma once
