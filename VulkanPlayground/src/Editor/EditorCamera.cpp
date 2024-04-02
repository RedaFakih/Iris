#include "vkPch.h"
#include "EditorCamera.h"

#include "Core/Input/Input.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace vkPlayground {

	namespace Utils {

		static void DisableMouse()
		{
			Input::SetCursorMode(CursorMode::Locked);
		}

		static void EnableMouse()
		{
			Input::SetCursorMode(CursorMode::Normal);
		}

	}

	EditorCamera::EditorCamera(float degFov, float width, float height, float nearClip, float farClip)
		: Camera(glm::perspectiveFov(glm::radians(degFov), width, height, farClip, nearClip), glm::perspectiveFov(glm::radians(degFov), width, height, nearClip, farClip)),
		  m_FOV(glm::radians(degFov)),
		  m_AspectRatio(width / height), 
		  m_NearClip(nearClip), 
		  m_FarClip(farClip)
	{
		Init();
	}

	void EditorCamera::Init()
	{
		const glm::vec3 position = { -5, 5, 5 };
		m_Distance = glm::distance(position, m_FocalPoint);

		m_Yaw = 3.0f * glm::pi<float>() / 4.0f;
		m_Pitch = glm::pi<float>() / 4.0f;

		m_Position = CalculatePosition();
		const glm::quat orientation = GetOrientation();
		m_Direction = glm::eulerAngles(orientation) * (180.0f / glm::pi<float>());
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void EditorCamera::OnUpdate(TimeStep ts)
	{
		const auto& [x, y] = Input::GetMousePosition();
		const glm::vec2& mouse{ x, y };
		const glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.002f;

		if (!m_IsActive)
		{
			return;
		}

		if (Input::IsKeyDown(KeyCode::LeftAlt))
		{
			m_CameraType = CameraType::BlenderCam;

			if (Input::IsMouseButtonDown(MouseButton::Right))
			{
				Utils::DisableMouse();
				MouseRotate(delta);
			}
			else if (Input::IsMouseButtonDown(MouseButton::Left))
			{
				Utils::DisableMouse();
				MousePan(delta);
			}
			else if (Input::IsMouseButtonDown(MouseButton::Middle))
			{
				Utils::DisableMouse();
				MouseZoom((delta.x + delta.y) * 0.1f);
			}
			else
				Utils::EnableMouse();
		}
		else if (Input::IsMouseButtonDown(MouseButton::Right) && !Input::IsKeyDown(KeyCode::LeftAlt))
		{
			m_CameraType = CameraType::FirstPerson;
			Utils::DisableMouse();
			const float yawSign = GetUpDirection().y < 0.0f ? -1.0f : 1.0f;
			const glm::vec3 upDirection = { 0.0f, yawSign, 0.0f };

			float speed = GetCameraSpeed();

			if (Input::IsKeyDown(KeyCode::W))
				m_PositionDelta += m_Direction * speed * ts.GetMilliSeconds();
			else if (Input::IsKeyDown(KeyCode::S))
				m_PositionDelta -= m_Direction * speed * ts.GetMilliSeconds();

			if (Input::IsKeyDown(KeyCode::A))
				m_PositionDelta -= m_RightDirection * speed * ts.GetMilliSeconds();
			else if (Input::IsKeyDown(KeyCode::D))
				m_PositionDelta += m_RightDirection * speed * ts.GetMilliSeconds();

			if (Input::IsKeyDown(KeyCode::Q))
				m_PositionDelta -= upDirection * speed * ts.GetMilliSeconds();
			else if (Input::IsKeyDown(KeyCode::E))
				m_PositionDelta += upDirection * speed * ts.GetMilliSeconds();

			constexpr float maxRate = 0.12f;
			m_YawDelta += glm::clamp(yawSign * delta.x * RotationSpeed(), -maxRate, maxRate);
			m_PitchDelta += glm::clamp(delta.y * RotationSpeed(), -maxRate, maxRate);

			m_RightDirection = glm::cross(m_Direction, upDirection);

			m_Direction = glm::rotate(glm::normalize(glm::cross(glm::angleAxis(-m_PitchDelta, m_RightDirection),
				glm::angleAxis(-m_YawDelta, upDirection))), m_Direction);

			float distance = glm::distance(m_FocalPoint, m_Position);
			m_FocalPoint = m_Position + GetForwardDirection() * distance;
			m_Distance = distance;
		}
		else
		{
			Utils::EnableMouse();
		}

		m_InitialMousePosition = mouse;
		constexpr float smoothing = 0.9f; // Decrease the smoothing a bit
		m_Position += m_PositionDelta * smoothing;
		m_Yaw += m_YawDelta * smoothing;
		m_Pitch += m_PitchDelta * smoothing;
		m_FocalPoint += m_FocalPointDelta * smoothing;

		if (m_CameraType == CameraType::BlenderCam)
			m_Position = CalculatePosition();

		UpdateView();
	}

	void EditorCamera::OnEvent(Events::Event& e)
	{
		Events::EventDispatcher dispatcher(e);
		dispatcher.Dispatch<Events::MouseScrolledEvent>([this](Events::MouseScrolledEvent& e) { return OnMouseScroll(e); });
	}

	void EditorCamera::Focus(const glm::vec3& focusPoint)
	{
		m_FocalPoint = focusPoint;
		m_CameraType = CameraType::FirstPerson;
		if (m_Distance > m_MinFocusDistance)
		{
			m_Distance -= m_Distance - m_MinFocusDistance;
			m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
		}
		m_Position = m_FocalPoint - GetForwardDirection() * m_Distance;
		UpdateView();
	}

	void EditorCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (m_ViewportWidth == width && m_ViewportHeight == height)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;
		SetPerspectiveProjectionMatrix(glm::degrees(m_FOV), (float)width, (float)height, m_NearClip, m_FarClip);
	}

	void EditorCamera::SetFOV(float degFov)
	{
		m_FOV = glm::radians(degFov);
		SetPerspectiveProjectionMatrix(degFov, (float)m_ViewportWidth, (float)m_ViewportHeight, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView()
	{
		const float yawSign = GetUpDirection().y < 0.0f ? -1.0f : 1.0f;

		// Extra step to handle the problem when the camera direction is the same as the up vector
		const float cosAngle = glm::dot(GetForwardDirection(), GetUpDirection());
		if (cosAngle * yawSign > 0.99f)
			m_PitchDelta = 0.0f;

		const glm::vec3 lookAt = m_Position + GetForwardDirection();
		m_Direction = glm::normalize(lookAt - m_Position);
		m_Distance = glm::distance(m_Position, m_FocalPoint);
		m_ViewMatrix = glm::lookAt(m_Position, lookAt, glm::vec3{ 0.0f, yawSign, 0.0f });

		//damping for smooth camera
		constexpr float damping = 0.82f;
		m_YawDelta *= damping;
		m_PitchDelta *= damping;
		m_PositionDelta *= damping;
		m_FocalPointDelta *= damping;
	}

	bool EditorCamera::OnMouseScroll(Events::MouseScrolledEvent& e)
	{
		if (Input::IsMouseButtonPressed(MouseButton::Right))
		{
			m_NormalSpeed += e.GetYOffset() * 0.3f * m_NormalSpeed;
			m_NormalSpeed = std::clamp(m_NormalSpeed, MIN_SPEED, MAX_SPEED);
		}
		else
		{
			MouseZoom(e.GetYOffset() * 0.1f);
			UpdateView();
		}

		return true;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPointDelta -= GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPointDelta += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		const float yawSign = GetUpDirection().y < 0.0f ? -1.0f : 1.0f;
		m_YawDelta += yawSign * delta.x * RotationSpeed();
		m_PitchDelta += delta.y * RotationSpeed();
	}

	void EditorCamera::MouseZoom(float delta)
	{
		m_Distance -= delta * ZoomSpeed();
		const glm::vec3 forwardDir = GetForwardDirection();
		m_Position = m_FocalPoint - forwardDir * m_Distance;
		if (m_Distance < 1.0f)
		{
			m_FocalPoint += forwardDir * m_Distance;
			m_Distance = 1.0f;
		}
		m_PositionDelta += delta * ZoomSpeed() * forwardDir;
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const
	{
		return 0.3f;
	}

	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = glm::max(distance, 0.0f);
		float speed = distance * distance;
		speed = glm::min(speed, 50.0f); // max speed = 50

		return speed;
	}

	float EditorCamera::GetCameraSpeed() const
	{
		float speed = m_NormalSpeed;
		if (Input::IsKeyDown(KeyCode::LeftControl))
			speed /= 2 - glm::log(m_NormalSpeed);

		if (Input::IsKeyDown(KeyCode::LeftShift))
			speed *= 2 - glm::log(m_NormalSpeed);

		return glm::clamp(speed, MIN_SPEED, MAX_SPEED);
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance + m_PositionDelta;
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch - m_PitchDelta, -m_Yaw - m_YawDelta, 0.0f));
	}

}