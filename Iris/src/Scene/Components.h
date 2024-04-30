#pragma once

#include "Core/UUID.h"
#include "Renderer/Mesh/Mesh.h"
#include "SceneCamera.h"
#include "Utils/Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <string>

namespace Iris {

	struct IDComponent
	{
		UUID ID = 0;
	};

	struct TagComponent
	{
		std::string Tag;
	};

	struct RelationshipComponent
	{
		UUID ParentHandle = 0;
		std::vector<UUID> Children;
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
	private: // Stored as private since we would want both representation to ensure precision and usability
		glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
	public:
		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation) * glm::toMat4(Rotation) * glm::scale(glm::mat4(1.0f), Scale);
		}

		void SetTransform(const glm::mat4& transformMat)
		{
			Math::DecomposeTransform(transformMat, Translation, Rotation, Scale);
			RotationEuler = glm::eulerAngles(Rotation);
		}

		glm::vec3 GetRotationEuler() const
		{
			return RotationEuler;
		}

		void SetRotationEuler(const glm::vec3& euler)
		{
			RotationEuler = euler;
			Rotation = glm::quat(euler);
		}

		glm::quat GetRotation() const
		{
			return Rotation;
		}

		void SetRotation(const glm::quat& rotation)
		{
			glm::vec3 originalEuler = RotationEuler;
			Rotation = rotation;
			RotationEuler = glm::eulerAngles(Rotation);

			if (
				(fabs(RotationEuler.x - originalEuler.x) == glm::pi<float>()) &&
				(fabs(RotationEuler.z - originalEuler.z) == glm::pi<float>())
			)
			{
				RotationEuler.x = originalEuler.x;
				RotationEuler.y = glm::pi<float>() - RotationEuler.y;
				RotationEuler.z = originalEuler.z;
			}
		}

		friend class SceneSerializer;
	};

	struct CameraComponent
	{
		// Type of camera is stored in the camera itself
		SceneCamera Camera;
		bool Primary = false;
	};

	// Renderer2D Quad Component
	struct SpriteRendererComponent
	{
		glm::vec4 Color = glm::vec4{ 1.0f };
		AssetHandle Texture = 0;
		float TilingFactor = 1.0f;
		glm::vec2 UV0 = { 0.0f, 0.0f };
		glm::vec2 UV1 = { 1.0f, 1.0f };
	};

	// TODO: Renderer2D Filled/Lined Circle Component (Maybe add a flag to select whether we want to draw the circle as filled or as outline)
	// TODO: CircleRendererComponent

	struct StaticMeshComponent
	{
		AssetHandle StaticMesh = 0;
		Ref<MaterialTable> MaterialTable = MaterialTable::Create();
		bool Visible = true;
	};

	// TODO: DirectionalLight, SkyLight

	template<typename... Components>
	struct ComponentGroup
	{
	};

	using AllComponents = ComponentGroup<IDComponent, TagComponent, RelationshipComponent, TransformComponent, CameraComponent, SpriteRendererComponent, StaticMeshComponent>;

}