#include "IrisPCH.h"
#include "Input.h"

#include "Core/Application.h"
#include "ImGui/ImGuiUtils.h"
#include "Renderer/StorageBufferSet.h"
#include "Renderer/UniformBufferSet.h"

#include <glfw/glfw3.h>
#include <imgui/imgui_internal.h>

namespace Iris {

	bool Input::IsKeyDown(KeyCode keyCode)
	{
		bool enableImGui = Application::Get().GetSpecification().EnableImGui;
		if (!enableImGui)
		{
			Window& window = Application::Get().GetWindow();
			auto state = glfwGetKey(window.GetNativeWindow(), static_cast<int32_t>(keyCode));
			return state == GLFW_PRESS || state == GLFW_REPEAT;
		}

		ImGuiContext* context = ImGui::GetCurrentContext();
		bool pressed = false;
		for (ImGuiViewport* viewport : context->Viewports)
		{
			if (!viewport->PlatformUserData)
				continue;

			GLFWwindow* windowHandle = *reinterpret_cast<GLFWwindow**>(viewport->PlatformUserData); // First member is GLFWwindow
			if (!windowHandle)
				continue;
			auto state = glfwGetKey(windowHandle, static_cast<int32_t>(keyCode));
			if (state == GLFW_PRESS || state == GLFW_REPEAT)
			{
				pressed = true;
				break;
			}
		}

		return pressed;
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

	bool Input::IsMouseButtonPressed(MouseButton button)
	{
		return s_ButtonData.find(button) != s_ButtonData.end() && s_ButtonData[button].State == KeyState::Pressed;
	}

	bool Input::IsMouseButtonHeld(MouseButton button)
	{
		return s_ButtonData.find(button) != s_ButtonData.end() && s_ButtonData[button].State == KeyState::Held;
	}

	bool Input::IsMouseButtonDown(MouseButton button)
	{
		bool enableImGui = Application::Get().GetSpecification().EnableImGui;
		if (!enableImGui)
		{
			Window& window = Application::Get().GetWindow();
			auto state = glfwGetMouseButton(window.GetNativeWindow(), static_cast<int32_t>(button));
			return state == GLFW_PRESS;
		}

		ImGuiContext* context = ImGui::GetCurrentContext();
		bool pressed = false;
		for (ImGuiViewport* viewport : context->Viewports)
		{
			if (!viewport->PlatformUserData)
				continue;

			GLFWwindow* windowHandle = *(GLFWwindow**)viewport->PlatformUserData; // First member is GLFWwindow
			if (!windowHandle)
				continue;

			auto state = glfwGetMouseButton(static_cast<GLFWwindow*>(windowHandle), static_cast<int32_t>(button));
			if (state == GLFW_PRESS || state == GLFW_REPEAT)
			{
				pressed = true;
				break;
			}
		}

		return pressed;
	}

	bool Input::IsMouseButtonReleased(MouseButton button)
	{
		return s_ButtonData.find(button) != s_ButtonData.end() && s_ButtonData[button].State == KeyState::Released;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		return { static_cast<float>(x), static_cast<float>(y) };
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

	void Input::SetCursorMode(CursorMode mode)
	{
		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL + static_cast<int>(mode));

		if (Application::Get().GetSpecification().EnableImGui)
			UI::SetInputEnabled(mode == CursorMode::Normal);
	}

	CursorMode Input::GetCursorMode()
	{
		GLFWwindow* window = Application::Get().GetWindow().GetNativeWindow();
		return static_cast<CursorMode>(glfwGetInputMode(window, GLFW_CURSOR) - GLFW_CURSOR_NORMAL);
	}

	void Input::TransitionPressedKeys()
	{
		for (const auto& [key, keyData] : s_KeyData)
		{
			if (keyData.State == KeyState::Pressed)
				UpdateKeyState(key, KeyState::Held);
		}
	}

	void Input::TransitionPressedButtons()
	{
		for (const auto& [button, buttonData] : s_ButtonData)
		{
			if (buttonData.State == KeyState::Pressed)
				UpdateButtonState(button, KeyState::Held);
		}
	}

	void Input::ClearReleasedKeys()
	{
		for (const auto& [key, keyData] : s_KeyData)
		{
			if (keyData.State == KeyState::Released)
				UpdateKeyState(key, KeyState::None);
		}

		for (const auto& [button, buttonData] : s_ButtonData)
		{
			if (buttonData.State == KeyState::Released)
				UpdateButtonState(button, KeyState::None);
		}
	}

	void Input::UpdateKeyState(KeyCode key, KeyState newState)
	{
		KeyData& keyData = s_KeyData[key];
		keyData.Key = key;
		keyData.OldState = keyData.State;
		keyData.State = newState;
	}

	void Input::UpdateButtonState(MouseButton button, KeyState newState)
	{
		ButtonData& mouseData = s_ButtonData[button];
		mouseData.Button = button;
		mouseData.OldState = mouseData.State;
		mouseData.State = newState;
	}

}