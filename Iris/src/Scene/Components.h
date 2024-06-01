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
			// wrap given euler angles to range [-pi, pi]
			auto wrapToPi = [](glm::vec3 v) constexpr
			{
				return glm::mod(v + glm::pi<float>(), 2.0f * glm::pi<float>()) - glm::pi<float>();
			};

			glm::vec3 originalEuler = RotationEuler;
			Rotation = rotation;
			RotationEuler = glm::eulerAngles(Rotation);

			// A given quat can be represented by many Euler angles (technically infinitely many),
			// and glm::eulerAngles() can only give us one of them which may or may not be the one we want.
			// Here we have a look at some likely alternatives and pick the one that is closest to the original Euler angles.
			// This is an attempt to avoid sudden 180deg flips in the Euler angles when we SetRotation(quat).

			glm::vec3 alternate1 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
			glm::vec3 alternate2 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z - glm::pi<float>() };
			glm::vec3 alternate3 = { RotationEuler.x + glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };
			glm::vec3 alternate4 = { RotationEuler.x - glm::pi<float>(), glm::pi<float>() - RotationEuler.y, RotationEuler.z + glm::pi<float>() };

			// We pick the alternative that is closest to the original value.
			float distance0 = glm::length2(wrapToPi(RotationEuler - originalEuler));
			float distance1 = glm::length2(wrapToPi(alternate1 - originalEuler));
			float distance2 = glm::length2(wrapToPi(alternate2 - originalEuler));
			float distance3 = glm::length2(wrapToPi(alternate3 - originalEuler));
			float distance4 = glm::length2(wrapToPi(alternate4 - originalEuler));

			float best = distance0;
			if (distance1 < best)
			{
				best = distance1;
				RotationEuler = alternate1;
			}
			if (distance2 < best)
			{
				best = distance2;
				RotationEuler = alternate2;
			}
			if (distance3 < best)
			{
				best = distance3;
				RotationEuler = alternate3;
			}
			if (distance4 < best)
			{
				best = distance4;
				RotationEuler = alternate4;
			}

			RotationEuler = wrapToPi(RotationEuler);
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
		uint32_t SubMeshIndex = 0;
		Ref<MaterialTable> MaterialTable = MaterialTable::Create();
		bool Visible = true;
	};

	// TODO: DirectionalLight, SkyLight (Actually handle the scene env)
	struct SkyLightComponent
	{
		AssetHandle SceneEnvironment;
		float Intensity = 1.0f;
		float Lod = 0.0f;

		bool DynamicSky = false;
		glm::vec3 TurbidityAzimuthInclination = { 2.0, 0.0, 0.0 };
	};

	template<typename... Components>
	struct ComponentGroup
	{
	};

	using AllComponents = ComponentGroup<IDComponent, TagComponent, RelationshipComponent, TransformComponent, CameraComponent, SpriteRendererComponent, StaticMeshComponent, SkyLightComponent>;

}