#include "vkPch.h"
#include "Camera.h"

namespace vkPlayground {

	Camera::Camera(const glm::mat4& projection)
		: m_Projection(projection)
	{
	}

	Camera::Camera(float degFov, float width, float height, float nearClip, float farClip)
		: m_Projection(glm::perspective(glm::radians(degFov), width / height, nearClip, farClip))
	{
	}

	void Camera::SetProjectionMatrix(const glm::mat4& projection)
	{
		m_Projection = projection;
	}

	void Camera::SetPerspectiveProjectionMatrix(float degFov, float width, float height, float nearClip, float farClip)
	{
		m_Projection = glm::perspective(glm::radians(degFov), width / height, nearClip, farClip);
	}

	void Camera::SetOrthographicProjectionMatrix(float width, float height, float nearClip, float farClip)
	{
		m_Projection = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearClip, farClip);
	}

}