#include "vkPch.h"
#include "Input.h"

#include "Core/Application.h"

#include <glfw/glfw3.h>
#include <imgui/imgui_internal.h>

namespace vkPlayground {

	bool Input::IsKeyDown(KeyCode keyCode)
	{
		//if (!(Application::Get().GetSpecification().EnableImGui))
		{
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();

			int state = glfwGetKey(window, (int)keyCode);

			return state == GLFW_PRESS || state == GLFW_REPEAT;
		}
	}

	bool Input::IsKeyPressed(KeyCode keyCode)
	{
		return s_KeyData.find(keyCode) != s_KeyData.end() && s_KeyData[keyCode].State == KeyState::Pressed;
	}

	bool Input::IsKeyHeld(KeyCode keyCode)
	{
		return s_KeyData.find(keyCode) != s_KeyData.end() && s_KeyData[keyCode].State == KeyState::Held;
	}

	bool Input::IsKeyReleased(KeyCode keyCode)
	{
		return s_KeyData.find(keyCode) != s_KeyData.end() && s_KeyData[keyCode].State == KeyState::Released;
	}

	bool Input::IsMouseButtonPressed(MouseButton mouseCode)
	{
		//if (!(Application::Get().GetSpecification().EnableImGui))
		{
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();

			int state = glfwGetMouseButton(window, (int)mouseCode);

			return state == GLFW_PRESS;
		}
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		return { (float)x, (float)y };
	}

	float Input::GetMouseX()
	{
		auto [x, y] = GetMousePosition();

		return x;
	}

	float Input::GetMouseY()
	{
		auto [x, y] = GetMousePosition();

		return y;
	}

	void Input::Update()
	{
		// Cleanup disconnected controllers.
		for (auto it = s_Controllers.begin(); it != s_Controllers.end();)
		{
			int id = it->first;
			if (glfwJoystickPresent(id) == GLFW_TRUE)
				it++;
			else
				it = s_Controllers.erase(it);
		}

		// Update Controllers.
		for (int id = GLFW_JOYSTICK_1; id < GLFW_JOYSTICK_LAST; id++)
		{
			if (glfwJoystickPresent(id) == GLFW_TRUE)
			{
				Controller& controller = s_Controllers[id];
				controller.ID = id;
				controller.Name = glfwGetJoystickName(id);

				int buttonCount;
				const uint8_t* buttons = glfwGetJoystickButtons(id, &buttonCount);
				for (int i = 0; i < buttonCount; i++)
					controller.ButtonStates[i] = buttons[i] == GLFW_PRESS;

				int axisCount;
				const float* axes = glfwGetJoystickAxes(id, &axisCount);
				for (int i = 0; i < axisCount; i++)
					controller.AxisStates[i] = axes[i];

				int hatCount;
				const uint8_t* hats = glfwGetJoystickHats(id, &hatCount);
				for (int i = 0; i < hatCount; i++)
					controller.HatStates[i] = hats[i];
			}
		}
	}

	bool Input::IsControllerPresent(int id)
	{
		return s_Controllers.find(id) != s_Controllers.end();
	}

	std::vector<int> Input::GetConnectedControllerIDs()
	{
		std::vector<int> ids;
		ids.reserve(s_Controllers.size());

		for (auto& [id, controller] : s_Controllers)
			ids.emplace_back(id);

		return ids;
	}

	const Controller* Input::GetController(int id)
	{
		if (!Input::IsControllerPresent(id))
			return nullptr;

		return &(s_Controllers.at(id));
	}

	std::string_view Input::GetControllerName(int id)
	{
		if (!Input::IsControllerPresent(id))
			return {};

		return s_Controllers.at(id).Name;
	}

	bool Input::IsControllerButtonPressed(int id, int button)
	{
		if (!Input::IsControllerPresent(id))
			return false;

		const Controller& controller = s_Controllers.at(id);
		if (controller.ButtonStates.find(button) == controller.ButtonStates.end())
			return false;

		return controller.ButtonStates.at(button);
	}

	float Input::GetControllerAxis(int id, int axis)
	{
		if (!Input::IsControllerPresent(id))
			return 0.0f;

		const Controller& controller = s_Controllers.at(id);
		if (controller.AxisStates.find(axis) == controller.AxisStates.end())
			return 0.0f;

		return controller.AxisStates.at(axis);
	}

	uint8_t Input::GetControllerHat(int id, int hat)
	{
		if (!Input::IsControllerPresent(id))
			return 0;

		const Controller& controller = s_Controllers.at(id);
		if (controller.HatStates.find(hat) == controller.HatStates.end())
			return 0;

		return controller.HatStates.at(hat);
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL + (int)mode);
	}

	CursorMode Input::GetCursorMode()
	{
		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		return (CursorMode)(glfwGetInputMode(window, GLFW_CURSOR) - GLFW_CURSOR_NORMAL);
	}

	void Input::TransitionPressedKeys()
	{
		for (const auto& [key, keyData] : s_KeyData)
		{
			if (keyData.State == KeyState::Pressed)
				UpdateKeyState(key, KeyState::Held);;
		}
	}

	void Input::ClearReleasedKeys()
	{
		for (const auto& [key, keyData] : s_KeyData)
		{
			if (keyData.State == KeyState::Released)
				UpdateKeyState(key, KeyState::None);
		}
	}

	void Input::UpdateKeyState(KeyCode key, KeyState newState)
	{
		KeyData& keyData = s_KeyData[key];
		keyData.Key = key;
		keyData.OldState = keyData.State;
		keyData.State = newState;
	}

	std::map<KeyCode, KeyData> Input::s_KeyData;
	std::map<int, Controller> Input::s_Controllers;

}