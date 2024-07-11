#pragma once

#include <glm/glm.hpp>

namespace Iris {

	class Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection, const glm::mat4& unReversedProjection);
		Camera(float degFov, float width, float height, float nearClip, float farClip);
		virtual ~Camera() = default;

		void SetProjectionMatrix(const glm::mat4& projection, const glm::mat4& unReversedProjection);
		void SetPerspectiveProjectionMatrix(float degFov, float width, float height, float nearClip, float farClip);
		void SetOrthographicProjectionMatrix(float size, float nearClip, float farClip, float aspectRatio);

		const glm::mat4& GetProjectionMatrix() const { return IsPerspectiveProjection() ? m_PerspectiveMatrix : m_OrthographicMatrix; }
		const glm::mat4& GetUnReversedProjectionMatrix() const { return IsPerspectiveProjection() ? m_PerspectiveUnReversedMatrix : m_OrthographicUnReversedMatrix; }

		bool IsPerspectiveProjection() const { return m_ProjectionType == ProjectionType::Perspective; }
		void SetPerspectiveProjection() { m_ProjectionType = ProjectionType::Perspective; }
		void SetOrthographicProjection() { m_ProjectionType = ProjectionType::Orthographic; }

		float& GetExposure() { return m_Exposure; }
		float GetExposure() const { return m_Exposure; }

	private:
		// NOTE: The projection matrix that is used for rendering has the near clip and the far clip reversed so that we can clear the depth buffer to 0.0f and use >= depth compare operation
		glm::mat4 m_PerspectiveMatrix = glm::mat4{ 1.0f };
		glm::mat4 m_PerspectiveUnReversedMatrix = glm::mat4{ 1.0f };

		glm::mat4 m_OrthographicMatrix = glm::mat4{ 1.0f };
		glm::mat4 m_OrthographicUnReversedMatrix = glm::mat4{ 1.0f };

		enum class ProjectionType { Perspective, Orthographic };
		ProjectionType m_ProjectionType = ProjectionType::Perspective;

		float m_Exposure = 1.0f;

	};

}