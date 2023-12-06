#pragma once

#include "Core/Base.h"
#include "KeyCodes.h"
#include "MouseCodes.h"

#include <utility>
#include <map>

namespace vkPlayground {

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

	struct Controller
	{
		int ID;
		std::string Name;
		std::map<int, bool> ButtonStates;
		std::map<int, float> AxisStates;
		std::map<int, uint8_t> HatStates;
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
		static bool IsMouseButtonPressed(MouseButton mouseCode);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		// Controller
		static bool IsControllerPresent(int id);
		static std::vector<int> GetConnectedControllerIDs();
		static const Controller* GetController(int id);
		static std::string_view GetControllerName(int id);
		static bool IsControllerButtonPressed(int id, int button);
		static float GetControllerAxis(int id, int axis);
		static uint8_t GetControllerHat(int id, int hat);
		static const std::map<int, Controller>& GetControllers() { return s_Controllers; }

		static void SetCursorMode(CursorMode mode);
		[[nodiscard]] static CursorMode GetCursorMode();

	private:
		static void TransitionPressedKeys();
		static void ClearReleasedKeys();
		static void UpdateKeyState(KeyCode key, KeyState newState);
		static void Update();

	private:
		static std::map<KeyCode, KeyData> s_KeyData;
		static std::map<int, Controller> s_Controllers;

		friend class Application;
		friend class Window;

	};

}