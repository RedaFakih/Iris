#pragma once

#include <ostream>

namespace vkPlayground {

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
#define VKPG_KEY_SPACE              ::vkPlayground::KeyCode::Space
#define VKPG_KEY_APOSTROPHE         ::vkPlayground::KeyCode::Apostrophe
#define VKPG_KEY_COMMA              ::vkPlayground::KeyCode::Comma
#define VKPG_KEY_MINUS              ::vkPlayground::KeyCode::Minus
#define VKPG_KEY_PERIOD             ::vkPlayground::KeyCode::Period
#define VKPG_KEY_SLASH              ::vkPlayground::KeyCode::Slash
#define VKPG_KEY_0                  ::vkPlayground::KeyCode::Key0
#define VKPG_KEY_1                  ::vkPlayground::KeyCode::Key1
#define VKPG_KEY_2                  ::vkPlayground::KeyCode::Key2
#define VKPG_KEY_3                  ::vkPlayground::KeyCode::Key3
#define VKPG_KEY_4                  ::vkPlayground::KeyCode::Key4
#define VKPG_KEY_5                  ::vkPlayground::KeyCode::Key5
#define VKPG_KEY_6                  ::vkPlayground::KeyCode::Key6
#define VKPG_KEY_7                  ::vkPlayground::KeyCode::Key7
#define VKPG_KEY_8                  ::vkPlayground::KeyCode::Key8
#define VKPG_KEY_9                  ::vkPlayground::KeyCode::Key9
#define VKPG_KEY_SEMICOLON          ::vkPlayground::KeyCode::SemiColon
#define VKPG_KEY_EQUAL              ::vkPlayground::KeyCode::Equal
#define VKPG_KEY_A                  ::vkPlayground::KeyCode::A
#define VKPG_KEY_B                  ::vkPlayground::KeyCode::B
#define VKPG_KEY_C                  ::vkPlayground::KeyCode::C
#define VKPG_KEY_D                  ::vkPlayground::KeyCode::D
#define VKPG_KEY_E                  ::vkPlayground::KeyCode::E
#define VKPG_KEY_F                  ::vkPlayground::KeyCode::F
#define VKPG_KEY_G                  ::vkPlayground::KeyCode::G
#define VKPG_KEY_H                  ::vkPlayground::KeyCode::H
#define VKPG_KEY_I                  ::vkPlayground::KeyCode::I
#define VKPG_KEY_J                  ::vkPlayground::KeyCode::J
#define VKPG_KEY_K                  ::vkPlayground::KeyCode::K
#define VKPG_KEY_L                  ::vkPlayground::KeyCode::L
#define VKPG_KEY_M                  ::vkPlayground::KeyCode::M
#define VKPG_KEY_N                  ::vkPlayground::KeyCode::N
#define VKPG_KEY_O                  ::vkPlayground::KeyCode::O
#define VKPG_KEY_P                  ::vkPlayground::KeyCode::P
#define VKPG_KEY_Q                  ::vkPlayground::KeyCode::Q
#define VKPG_KEY_R                  ::vkPlayground::KeyCode::R
#define VKPG_KEY_S                  ::vkPlayground::KeyCode::S
#define VKPG_KEY_T                  ::vkPlayground::KeyCode::T
#define VKPG_KEY_U                  ::vkPlayground::KeyCode::U
#define VKPG_KEY_V                  ::vkPlayground::KeyCode::V
#define VKPG_KEY_W                  ::vkPlayground::KeyCode::W
#define VKPG_KEY_X                  ::vkPlayground::KeyCode::X
#define VKPG_KEY_Y                  ::vkPlayground::KeyCode::Y
#define VKPG_KEY_Z                  ::vkPlayground::KeyCode::Z
#define VKPG_KEY_LEFT_BRACKET       ::vkPlayground::KeyCode::LeftBracket
#define VKPG_KEY_BACKSLASH          ::vkPlayground::KeyCode::BackSlash
#define VKPG_KEY_RIGHT_BRACKET      ::vkPlayground::KeyCode::RightBracket
#define VKPG_KEY_GRAVE_ACCENT       ::vkPlayground::KeyCode::GraveAccent
#define VKPG_KEY_WORLD_1            ::vkPlayground::KeyCode::World1
#define VKPG_KEY_WORLD_2            ::vkPlayground::KeyCode::World2

// Function
#define VKPG_KEY_ESCAPE             ::vkPlayground::KeyCode::Escape
#define VKPG_KEY_ENTER              ::vkPlayground::KeyCode::Enter
#define VKPG_KEY_TAB                ::vkPlayground::KeyCode::Tab
#define VKPG_KEY_BACKSPACE          ::vkPlayground::KeyCode::Backspace
#define VKPG_KEY_INSERT             ::vkPlayground::KeyCode::Insert
#define VKPG_KEY_DELETE             ::vkPlayground::KeyCode::Delete
#define VKPG_KEY_RIGHT              ::vkPlayground::KeyCode::Right
#define VKPG_KEY_LEFT               ::vkPlayground::KeyCode::Left
#define VKPG_KEY_DOWN               ::vkPlayground::KeyCode::Down
#define VKPG_KEY_UP                 ::vkPlayground::KeyCode::Up
#define VKPG_KEY_PAGE_UP            ::vkPlayground::KeyCode::PageUp
#define VKPG_KEY_PAGE_DOWN          ::vkPlayground::KeyCode::PageDown
#define VKPG_KEY_HOME               ::vkPlayground::KeyCode::Home
#define VKPG_KEY_END                ::vkPlayground::KeyCode::End
#define VKPG_KEY_CAPS_LOCK          ::vkPlayground::KeyCode::CapsLock
#define VKPG_KEY_SCROLL_LOCK        ::vkPlayground::KeyCode::ScrollLock
#define VKPG_KEY_NUM_LOCK           ::vkPlayground::KeyCode::NumLock
#define VKPG_KEY_PRINT_SCREEN       ::vkPlayground::KeyCode::PrintScreen
#define VKPG_KEY_PAUSE              ::vkPlayground::KeyCode::Pause
#define VKPG_KEY_F1                 ::vkPlayground::KeyCode::F1
#define VKPG_KEY_F2                 ::vkPlayground::KeyCode::F2
#define VKPG_KEY_F3                 ::vkPlayground::KeyCode::F3
#define VKPG_KEY_F4                 ::vkPlayground::KeyCode::F4
#define VKPG_KEY_F5                 ::vkPlayground::KeyCode::F5
#define VKPG_KEY_F6                 ::vkPlayground::KeyCode::F6
#define VKPG_KEY_F7                 ::vkPlayground::KeyCode::F7
#define VKPG_KEY_F8                 ::vkPlayground::KeyCode::F8
#define VKPG_KEY_F9                 ::vkPlayground::KeyCode::F9
#define VKPG_KEY_F10                ::vkPlayground::KeyCode::F10
#define VKPG_KEY_F11                ::vkPlayground::KeyCode::F11
#define VKPG_KEY_F12                ::vkPlayground::KeyCode::F12
#define VKPG_KEY_F13                ::vkPlayground::KeyCode::F13
#define VKPG_KEY_F14                ::vkPlayground::KeyCode::F14
#define VKPG_KEY_F15                ::vkPlayground::KeyCode::F15
#define VKPG_KEY_F16                ::vkPlayground::KeyCode::F16
#define VKPG_KEY_F17                ::vkPlayground::KeyCode::F17
#define VKPG_KEY_F18                ::vkPlayground::KeyCode::F18
#define VKPG_KEY_F19                ::vkPlayground::KeyCode::F19
#define VKPG_KEY_F20                ::vkPlayground::KeyCode::F20
#define VKPG_KEY_F21                ::vkPlayground::KeyCode::F21
#define VKPG_KEY_F22                ::vkPlayground::KeyCode::F22
#define VKPG_KEY_F23                ::vkPlayground::KeyCode::F23
#define VKPG_KEY_F24                ::vkPlayground::KeyCode::F24
#define VKPG_KEY_F25                ::vkPlayground::KeyCode::F25

// Keypad
#define VKPG_KEY_KP_0               ::vkPlayground::KeyCode::KP0
#define VKPG_KEY_KP_1               ::vkPlayground::KeyCode::KP1
#define VKPG_KEY_KP_2               ::vkPlayground::KeyCode::KP2
#define VKPG_KEY_KP_3               ::vkPlayground::KeyCode::KP3
#define VKPG_KEY_KP_4               ::vkPlayground::KeyCode::KP4
#define VKPG_KEY_KP_5               ::vkPlayground::KeyCode::KP5
#define VKPG_KEY_KP_6               ::vkPlayground::KeyCode::KP6
#define VKPG_KEY_KP_7               ::vkPlayground::KeyCode::KP7
#define VKPG_KEY_KP_8               ::vkPlayground::KeyCode::KP8
#define VKPG_KEY_KP_9               ::vkPlayground::KeyCode::KP9
#define VKPG_KEY_KP_DECIMAL         ::vkPlayground::KeyCode::KPDecimal
#define VKPG_KEY_KP_DIVIDE          ::vkPlayground::KeyCode::KPDivide
#define VKPG_KEY_KP_MULTIPLY        ::vkPlayground::KeyCode::KPMultiply
#define VKPG_KEY_KP_SUBTRACT        ::vkPlayground::KeyCode::KPSubtract
#define VKPG_KEY_KP_ADD             ::vkPlayground::KeyCode::KPAdd
#define VKPG_KEY_KP_ENTER           ::vkPlayground::KeyCode::KPEnter
#define VKPG_KEY_KP_EQUAL           ::vkPlayground::KeyCode::KPEqual

// Controls
#define VKPG_KEY_LEFT_SHIFT         ::vkPlayground::KeyCode::LeftShift
#define VKPG_KEY_LEFT_CONTROL       ::vkPlayground::KeyCode::LeftControl
#define VKPG_KEY_LEFT_ALT           ::vkPlayground::KeyCode::LeftAlt
#define VKPG_KEY_LEFT_SUPER         ::vkPlayground::KeyCode::LeftSuper
#define VKPG_KEY_RIGHT_SHIFT        ::vkPlayground::KeyCode::RightShift
#define VKPG_KEY_RIGHT_CONTROL      ::vkPlayground::KeyCode::RightControl
#define VKPG_KEY_RIGHT_ALT          ::vkPlayground::KeyCode::RightAlt
#define VKPG_KEY_RIGHT_SUPER        ::vkPlayground::KeyCode::RightSuper
#define VKPG_KEY_MENU               ::vkPlayground::KeyCode::Menu