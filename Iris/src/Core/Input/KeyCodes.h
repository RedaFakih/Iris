#pragma once

#include <ostream>

namespace Iris {

	enum class KeyCode : uint16_t
	{
		Space = 32,
		Apostrophe = 39,  /* ' */
		Comma = 44,  /* , */
		Minus = 45,  /* - */
		Period = 46,  /* . */
		Slash = 47,  /* / */

		// Digit keys
		Key0 = 48,
		Key1 = 49,
		Key2 = 50,
		Key3 = 51,
		Key4 = 52,
		Key5 = 53,
		Key6 = 54,
		Key7 = 55,
		Key8 = 56,
		Key9 = 57,

		SemiColon = 59,  /* ; */
		Equal = 61,  /* = */

		// Alphabet Keys
		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		BackSlash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		// Function Keys
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		// Keypad
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	};

	inline std::ostream& operator<<(std::ostream& stream, KeyCode code)
	{
		stream << (int)code;

		return stream;
	}

	enum class KeyState
	{
		None = -1,
		Pressed,
		Held,
		Released
	};

}

/* Printable keys */
/* Taken from glfw3.h */
#define IR_KEY_SPACE              ::Iris::KeyCode::Space
#define IR_KEY_APOSTROPHE         ::Iris::KeyCode::Apostrophe
#define IR_KEY_COMMA              ::Iris::KeyCode::Comma
#define IR_KEY_MINUS              ::Iris::KeyCode::Minus
#define IR_KEY_PERIOD             ::Iris::KeyCode::Period
#define IR_KEY_SLASH              ::Iris::KeyCode::Slash
#define IR_KEY_0                  ::Iris::KeyCode::Key0
#define IR_KEY_1                  ::Iris::KeyCode::Key1
#define IR_KEY_2                  ::Iris::KeyCode::Key2
#define IR_KEY_3                  ::Iris::KeyCode::Key3
#define IR_KEY_4                  ::Iris::KeyCode::Key4
#define IR_KEY_5                  ::Iris::KeyCode::Key5
#define IR_KEY_6                  ::Iris::KeyCode::Key6
#define IR_KEY_7                  ::Iris::KeyCode::Key7
#define IR_KEY_8                  ::Iris::KeyCode::Key8
#define IR_KEY_9                  ::Iris::KeyCode::Key9
#define IR_KEY_SEMICOLON          ::Iris::KeyCode::SemiColon
#define IR_KEY_EQUAL              ::Iris::KeyCode::Equal
#define IR_KEY_A                  ::Iris::KeyCode::A
#define IR_KEY_B                  ::Iris::KeyCode::B
#define IR_KEY_C                  ::Iris::KeyCode::C
#define IR_KEY_D                  ::Iris::KeyCode::D
#define IR_KEY_E                  ::Iris::KeyCode::E
#define IR_KEY_F                  ::Iris::KeyCode::F
#define IR_KEY_G                  ::Iris::KeyCode::G
#define IR_KEY_H                  ::Iris::KeyCode::H
#define IR_KEY_I                  ::Iris::KeyCode::I
#define IR_KEY_J                  ::Iris::KeyCode::J
#define IR_KEY_K                  ::Iris::KeyCode::K
#define IR_KEY_L                  ::Iris::KeyCode::L
#define IR_KEY_M                  ::Iris::KeyCode::M
#define IR_KEY_N                  ::Iris::KeyCode::N
#define IR_KEY_O                  ::Iris::KeyCode::O
#define IR_KEY_P                  ::Iris::KeyCode::P
#define IR_KEY_Q                  ::Iris::KeyCode::Q
#define IR_KEY_R                  ::Iris::KeyCode::R
#define IR_KEY_S                  ::Iris::KeyCode::S
#define IR_KEY_T                  ::Iris::KeyCode::T
#define IR_KEY_U                  ::Iris::KeyCode::U
#define IR_KEY_V                  ::Iris::KeyCode::V
#define IR_KEY_W                  ::Iris::KeyCode::W
#define IR_KEY_X                  ::Iris::KeyCode::X
#define IR_KEY_Y                  ::Iris::KeyCode::Y
#define IR_KEY_Z                  ::Iris::KeyCode::Z
#define IR_KEY_LEFT_BRACKET       ::Iris::KeyCode::LeftBracket
#define IR_KEY_BACKSLASH          ::Iris::KeyCode::BackSlash
#define IR_KEY_RIGHT_BRACKET      ::Iris::KeyCode::RightBracket
#define IR_KEY_GRAVE_ACCENT       ::Iris::KeyCode::GraveAccent
#define IR_KEY_WORLD_1            ::Iris::KeyCode::World1
#define IR_KEY_WORLD_2            ::Iris::KeyCode::World2

// Function
#define IR_KEY_ESCAPE             ::Iris::KeyCode::Escape
#define IR_KEY_ENTER              ::Iris::KeyCode::Enter
#define IR_KEY_TAB                ::Iris::KeyCode::Tab
#define IR_KEY_BACKSPACE          ::Iris::KeyCode::Backspace
#define IR_KEY_INSERT             ::Iris::KeyCode::Insert
#define IR_KEY_DELETE             ::Iris::KeyCode::Delete
#define IR_KEY_RIGHT              ::Iris::KeyCode::Right
#define IR_KEY_LEFT               ::Iris::KeyCode::Left
#define IR_KEY_DOWN               ::Iris::KeyCode::Down
#define IR_KEY_UP                 ::Iris::KeyCode::Up
#define IR_KEY_PAGE_UP            ::Iris::KeyCode::PageUp
#define IR_KEY_PAGE_DOWN          ::Iris::KeyCode::PageDown
#define IR_KEY_HOME               ::Iris::KeyCode::Home
#define IR_KEY_END                ::Iris::KeyCode::End
#define IR_KEY_CAPS_LOCK          ::Iris::KeyCode::CapsLock
#define IR_KEY_SCROLL_LOCK        ::Iris::KeyCode::ScrollLock
#define IR_KEY_NUM_LOCK           ::Iris::KeyCode::NumLock
#define IR_KEY_PRINT_SCREEN       ::Iris::KeyCode::PrintScreen
#define IR_KEY_PAUSE              ::Iris::KeyCode::Pause
#define IR_KEY_F1                 ::Iris::KeyCode::F1
#define IR_KEY_F2                 ::Iris::KeyCode::F2
#define IR_KEY_F3                 ::Iris::KeyCode::F3
#define IR_KEY_F4                 ::Iris::KeyCode::F4
#define IR_KEY_F5                 ::Iris::KeyCode::F5
#define IR_KEY_F6                 ::Iris::KeyCode::F6
#define IR_KEY_F7                 ::Iris::KeyCode::F7
#define IR_KEY_F8                 ::Iris::KeyCode::F8
#define IR_KEY_F9                 ::Iris::KeyCode::F9
#define IR_KEY_F10                ::Iris::KeyCode::F10
#define IR_KEY_F11                ::Iris::KeyCode::F11
#define IR_KEY_F12                ::Iris::KeyCode::F12
#define IR_KEY_F13                ::Iris::KeyCode::F13
#define IR_KEY_F14                ::Iris::KeyCode::F14
#define IR_KEY_F15                ::Iris::KeyCode::F15
#define IR_KEY_F16                ::Iris::KeyCode::F16
#define IR_KEY_F17                ::Iris::KeyCode::F17
#define IR_KEY_F18                ::Iris::KeyCode::F18
#define IR_KEY_F19                ::Iris::KeyCode::F19
#define IR_KEY_F20                ::Iris::KeyCode::F20
#define IR_KEY_F21                ::Iris::KeyCode::F21
#define IR_KEY_F22                ::Iris::KeyCode::F22
#define IR_KEY_F23                ::Iris::KeyCode::F23
#define IR_KEY_F24                ::Iris::KeyCode::F24
#define IR_KEY_F25                ::Iris::KeyCode::F25

// Keypad
#define IR_KEY_KP_0               ::Iris::KeyCode::KP0
#define IR_KEY_KP_1               ::Iris::KeyCode::KP1
#define IR_KEY_KP_2               ::Iris::KeyCode::KP2
#define IR_KEY_KP_3               ::Iris::KeyCode::KP3
#define IR_KEY_KP_4               ::Iris::KeyCode::KP4
#define IR_KEY_KP_5               ::Iris::KeyCode::KP5
#define IR_KEY_KP_6               ::Iris::KeyCode::KP6
#define IR_KEY_KP_7               ::Iris::KeyCode::KP7
#define IR_KEY_KP_8               ::Iris::KeyCode::KP8
#define IR_KEY_KP_9               ::Iris::KeyCode::KP9
#define IR_KEY_KP_DECIMAL         ::Iris::KeyCode::KPDecimal
#define IR_KEY_KP_DIVIDE          ::Iris::KeyCode::KPDivide
#define IR_KEY_KP_MULTIPLY        ::Iris::KeyCode::KPMultiply
#define IR_KEY_KP_SUBTRACT        ::Iris::KeyCode::KPSubtract
#define IR_KEY_KP_ADD             ::Iris::KeyCode::KPAdd
#define IR_KEY_KP_ENTER           ::Iris::KeyCode::KPEnter
#define IR_KEY_KP_EQUAL           ::Iris::KeyCode::KPEqual

// Controls
#define IR_KEY_LEFT_SHIFT         ::Iris::KeyCode::LeftShift
#define IR_KEY_LEFT_CONTROL       ::Iris::KeyCode::LeftControl
#define IR_KEY_LEFT_ALT           ::Iris::KeyCode::LeftAlt
#define IR_KEY_LEFT_SUPER         ::Iris::KeyCode::LeftSuper
#define IR_KEY_RIGHT_SHIFT        ::Iris::KeyCode::RightShift
#define IR_KEY_RIGHT_CONTROL      ::Iris::KeyCode::RightControl
#define IR_KEY_RIGHT_ALT          ::Iris::KeyCode::RightAlt
#define IR_KEY_RIGHT_SUPER        ::Iris::KeyCode::RightSuper
#define IR_KEY_MENU               ::Iris::KeyCode::Menu