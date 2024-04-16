#pragma once

#include <ostream>

namespace Iris {

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

		Left = Button0,
		Right = Button1,
		Middle = Button2
	};

	inline std::ostream& operator<<(std::ostream& stream, MouseButton code)
	{
		stream << (int)code;

		return stream;
	}

}

#define IR_MOUSE_BUTTON_0        ::Iris::MouseButton::Button0
#define IR_MOUSE_BUTTON_1        ::Iris::MouseButton::Button1
#define IR_MOUSE_BUTTON_2        ::Iris::MouseButton::Button2
#define IR_MOUSE_BUTTON_3        ::Iris::MouseButton::Button3
#define IR_MOUSE_BUTTON_4        ::Iris::MouseButton::Button4
#define IR_MOUSE_BUTTON_5        ::Iris::MouseButton::Button5
#define IR_MOUSE_BUTTON_6        ::Iris::MouseButton::Button6
#define IR_MOUSE_BUTTON_7        ::Iris::MouseButton::Button7

#define IR_MOUSE_BUTTON_LEFT     ::Iris::MouseButton::Left
#define IR_MOUSE_BUTTON_RIGHT    ::Iris::MouseButton::Right
#define IR_MOUSE_BUTTON_MIDDLE   ::Iris::MouseButton::Middle#pragma once
