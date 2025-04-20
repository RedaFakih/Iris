#include "IrisPCH.h"
#include "SceneCamera.h"

namespace Iris {

	void SceneCamera::SetPerspective(float degreeVerticalFOV, float nearClip, float farClip)
	{
		m_ProjectionType = ProjectionType::Perspective;
		m_DegPerspectiveFOV = degreeVerticalFOV;
		m_PerspectiveNear = nearClip;
		m_PerspectiveFar = farClip;
	}

	void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		m_ProjectionType = ProjectionType::Orthographic;
		m_OrthographicSize = size;
		m_OrthographicNear = nearClip;
		m_OrthographicFar = farClip;
	}

	void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		switch (m_ProjectionType)
		{
			case ProjectionType::Perspective:
				SetPerspectiveProjectionMatrix(m_DegPerspectiveFOV, static_cast<float>(width), static_cast<float>(height), m_PerspectiveNear, m_PerspectiveFar);
				break;
			case ProjectionType::Orthographic:
				float aspecRatio = static_cast<float>(width) / static_cast<float>(height);
				float newWidth = m_OrthographicSize * aspecRatio;
				float newHeight = m_OrthographicSize;
				SetOrthographicProjectionMatrix(newWidth, newHeight, m_OrthographicNear, m_OrthographicFar);
				break;
		}
	}

}