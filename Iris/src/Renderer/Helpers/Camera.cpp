#include "IrisPCH.h"
#include "Camera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace Iris {

	Camera::Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection)
		: m_PerspectiveMatrix(projection), m_PerspectiveUnReversedMatrix(unReversedProjection)
	{
		// Default orthographic matrices
		SetOrthographicProjectionMatrix(2.0f * 1.778f, 2.0f, 0.01f, 10000.0f);
	}

	Camera::Camera(float degFov, float width, float height, float nearClip, float farClip)
		: m_PerspectiveMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, farClip, nearClip)),
		m_PerspectiveUnReversedMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, nearClip, farClip))
	{
		// Default orthographic matrices
		SetOrthographicProjectionMatrix(2.0f * 1.778f, 2.0f, 0.01f, 10000.0f);
	}

	void Camera::SetProjectionMatrix(const glm::mat4& projection, const glm::mat4& unReversedProjection)
	{
		m_PerspectiveMatrix = projection;
		m_PerspectiveUnReversedMatrix = unReversedProjection;
	}

	void Camera::SetPerspectiveProjectionMatrix(float degFov, float width, float height, float nearClip, float farClip)
	{
		float radFov = glm::radians(degFov);

		m_PerspectiveMatrix = glm::perspectiveFov(radFov, width, height, farClip, nearClip);
		m_PerspectiveUnReversedMatrix = glm::perspectiveFov(radFov, width, height, nearClip, farClip);
	}

	void Camera::SetOrthographicProjectionMatrix(float width, float height, float nearClip, float farClip)
	{
		m_OrthographicMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, farClip, nearClip);
		m_OrthographicUnReversedMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearClip, farClip);
	}

}