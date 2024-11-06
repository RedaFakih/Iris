#pragma once

#include "Core/UUID.h"
#include "Physics/ColliderMaterial.h"
#include "Physics/PhysicsTypes.h"
#include "Renderer/Mesh/Mesh.h"
#include "SceneCamera.h"
#include "Utils/Math.h"

#include <box2d/box2d.h>

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

	struct VisibleComponent
	{
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
	};

	struct TextComponent
	{
		std::string TextString = "";
		size_t TextHash;

		AssetHandle Font = 0;
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float LineSpacing = 1.0f;
		float Kerning = 1.0f;

		float MaxWidth = 20.0f;
	};

	struct SkyLightComponent
	{
		AssetHandle SceneEnvironment = 0;
		float Intensity = 1.0f;
		float Lod = 0.0f;

		bool DynamicSky = false;
		glm::vec4 TurbidityAzimuthInclinationSunSize = { 2.0f, 0.0f, 0.0f, 0.01f };
		UUID DirectionalLightEntityID = 0;
		bool LinkDirectionalLightRadiance = false;
	};

	struct DirectionalLightComponent
	{
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		bool CastShadows = true;
		float ShadowOpacity = 1.0f;
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Physics
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/// 3D

	class PhysicsScene;

	struct JoltWorldComponent
	{
		Ref<PhysicsScene> PhysicsWorld = nullptr;
	};
	
	struct RigidBodyComponent
	{
		PhysicsBodyType BodyType = PhysicsBodyType::Static;
		uint32_t LayerID = 0;
		bool EnableDynamicTypeChange = false;

		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		bool DisableGravity = false;
		bool IsTrigger = false;
		PhysicsCollisionDetectionType CollisionDetection = PhysicsCollisionDetectionType::Discrete;

		glm::vec3 InitialLinearVelocity = { 0.0f, 0.0f, 0.0f };
		float MaxLinearVelocity = 500.0f;

		glm::vec3 InitialAngularVelocity = { 0.0f, 0.0f, 0.0f };
		float MaxAngularVelocity = 50.0f;

		PhysicsActorAxis LockedAxes = PhysicsActorAxis::None;
	};

	struct BoxColliderComponent
	{
		glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		ColliderMaterial Material;
	};

	struct SphereColliderComponent
	{
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		float Radius = 0.5f;

		ColliderMaterial Material;
	};

	struct CylinderColliderComponent
	{
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		float Radius = 0.5f;
		float HalfHeight = 0.5f;

		ColliderMaterial Material;
	};

	struct CapsuleColliderComponent
	{
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		float Radius = 0.5f;
		float HalfHeight = 0.5f;

		ColliderMaterial Material;
	};

	/*
	 * The way Compound Colliders work is that you can create a top level parent entity, add children to it that have ColliderComponents AND RigidBodyComponents,
	 * then add a CompoundColliderComponent to the top level parent entity AND a RigidBodyComponent and then that will combine all the child colliders
	 * into one collider
	 */
	struct CompoundColliderComponent
	{
		bool IsImmutable = true;
		bool IncludeStaticChildColliders = true;
		
		std::vector<UUID> CompoundedColliderEntities;
	};

	struct MeshColliderComponent
	{
		AssetHandle ColliderAsset = 0;
		uint32_t SubMeshIndex = 0;
		bool UseSharedShape = false; // TODO: What is this?
		ColliderMaterial Material;
		PhysicsCollisionComplexity CollisionComplexity = PhysicsCollisionComplexity::Default;
	};

	/// 2D

	struct Box2DWorldComponent
	{
		Scope<b2World> World;
	};

	struct RigidBody2DComponent
	{
		PhysicsBodyType BodyType = PhysicsBodyType::Static;

		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		float GravityScale = 1.0f;
		bool FixedRotation = false;

		void* RuntimeBody = nullptr;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 HalfSize = { 0.5f, 0.5f };
		glm::vec2 Offset = { 0.0f, 0.0f };

		float Density = 1.0f;
		ColliderMaterial Material;
		float RestitutionThreshold = 0.5f;

		void* RuntimeFixture = nullptr;
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 0.5f;

		float Density = 1.0f;
		ColliderMaterial Material;
		float RestitutionThreshold = 0.5f;

		void* RuntimeFixture = nullptr;
	};

	template<typename... Components>
	struct ComponentGroup
	{
	};

	using AllComponents = ComponentGroup<IDComponent, TagComponent, RelationshipComponent, TransformComponent, CameraComponent, SpriteRendererComponent, StaticMeshComponent, 
		TextComponent, SkyLightComponent, DirectionalLightComponent, RigidBody2DComponent, BoxCollider2DComponent>;

}