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
		void SetOrthographicProjectionMatrix(float width, float height, float nearClip, float farClip);

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetUnReversedProjectionMatrix() const { return m_UnReversedProjectionMatrix; }

		float& GetExposure() { return m_Exposure; }
		float GetExposure() const { return m_Exposure; }

	private:
		// NOTE: The projection matrix that is used for rendering has the near clip and the far clip reversed so that we can clear the depth buffer to 0.0f and use >= depth compare operation
		glm::mat4 m_ProjectionMatrix = glm::mat4{ 1.0f };
		glm::mat4 m_UnReversedProjectionMatrix = glm::mat4{ 1.0f };

		float m_Exposure = 0.8f;

	};

}