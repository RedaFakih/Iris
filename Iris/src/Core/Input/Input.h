#pragma once

#include "Core/Base.h"
#include "KeyCodes.h"
#include "MouseCodes.h"

#include <utility>
#include <map>

namespace Iris {

	enum class CursorMode : uint8_t
	{
		Normal = 0,
		Hidden,
		Locked
	};

	struct KeyData
	{
		KeyCode Key;
		KeyState State = KeyState::None;
		KeyState OldState = KeyState::None;
	};

	struct ButtonData
	{
		MouseButton Button;
		KeyState State = KeyState::None;
		KeyState OldState = KeyState::None;
	};

	class Input
	{
	public:
		// Key
		static bool IsKeyPressed(KeyCode keyCode);
		static bool IsKeyHeld(KeyCode keyCode);
		static bool IsKeyDown(KeyCode keyCode);
		static bool IsKeyReleased(KeyCode keyCode);

		// Mouse
		static bool IsMouseButtonPressed(MouseButton button);
		static bool IsMouseButtonHeld(MouseButton button);
		static bool IsMouseButtonDown(MouseButton button);
		static bool IsMouseButtonReleased(MouseButton button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		static void SetCursorMode(CursorMode mode);
		[[nodiscard]] static CursorMode GetCursorMode();

	private:
		static void TransitionPressedKeys();
		static void TransitionPressedButtons();
		static void ClearReleasedKeys();
		static void UpdateKeyState(KeyCode key, KeyState newState);
		static void UpdateButtonState(MouseButton button, KeyState newState);

	private:
		inline static std::map<KeyCode, KeyData> s_KeyData;
		inline static std::map<MouseButton, ButtonData> s_ButtonData;

		friend class Application;
		friend class Window;

	};

}