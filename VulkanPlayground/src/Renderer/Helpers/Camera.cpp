#include "vkPch.h"
#include "Camera.h"

#include <glm/ext/matrix_clip_space.hpp>

namespace vkPlayground {

	Camera::Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection)
		: m_ProjectionMatrix(projection), m_UnReversedProjectionMatrix(unReversedProjection)
	{
	}

	Camera::Camera(float degFov, float width, float height, float nearClip, float farClip)
		: m_ProjectionMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, farClip, nearClip)), 
		  m_UnReversedProjectionMatrix(glm::perspectiveFov(glm::radians(degFov), width, height, nearClip, farClip))
	{
	}

	void Camera::SetProjectionMatrix(const glm::mat4& projection, const glm::mat4& unReversedProjection)
	{
		m_ProjectionMatrix = projection;
		m_UnReversedProjectionMatrix = unReversedProjection;
	}

	void Camera::SetPerspectiveProjectionMatrix(float degFov, float width, float height, float nearClip, float farClip)
	{
		float radFov = glm::radians(degFov);

		m_ProjectionMatrix = glm::perspectiveFov(radFov, width, height, farClip, nearClip);
		m_UnReversedProjectionMatrix = glm::perspectiveFov(radFov, width, height, nearClip, farClip);
	}

	void Camera::SetOrthographicProjectionMatrix(float width, float height, float nearClip, float farClip)
	{
		m_ProjectionMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, farClip, nearClip);
		m_UnReversedProjectionMatrix = glm::ortho(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearClip, farClip);
	}

}