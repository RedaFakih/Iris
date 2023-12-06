#pragma once

#include <glm/glm.hpp>

namespace vkPlayground {

	class Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection);
		Camera(float degFov, float width, float height, float nearClip, float farClip);
		virtual ~Camera() = default;

		void SetProjectionMatrix(const glm::mat4& projection);
		void SetPerspectiveProjectionMatrix(float degFov, float width, float height, float nearClip, float farClip);
		void SetOrthographicProjectionMatrix(float width, float height, float nearClip, float farClip);

		const glm::mat4& GetProjection() const { return m_Projection; }

	private:
		glm::mat4 m_Projection = glm::mat4{ 1.0f };
	};

}