#include "IrisPCH.h"
#include "Camera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace Iris {

	Camera::Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection)
		: m_PerspectiveMatrix(projection), m_PerspectiveUnReversedMatrix(unReversedProjection)
	{
		// Default orthographic matrices
		SetOrthographicProjectionMatrix(1.0f, 0.0f, 10000.0f, 1.778f);
	}

	Camera::Camera(float degFov, float width, float height, float nearClip, float farClip)
		: m_PerspectiveMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, farClip, nearClip)),
		m_PerspectiveUnReversedMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, nearClip, farClip))
	{
		// Default orthographic matrices
		SetOrthographicProjectionMatrix(1.0f, 0.0f, 10000.0f, 1.778f);
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

	void Camera::SetOrthographicProjectionMatrix(float size, float nearClip, float farClip, float aspectRatio)
	{
		float orthoLeft = -size * 0.5f;
		float orthoRight = size * 0.5f;
		float orthoBottom = -size * 0.5f;
		float orthoTop = size * 0.5f;

		m_OrthographicMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, farClip, nearClip);
		m_OrthographicUnReversedMatrix = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearClip, farClip);
	}

}