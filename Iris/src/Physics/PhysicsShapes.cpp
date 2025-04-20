#include "IrisPCH.h"
#include "PhysicsShapes.h"

#include "AssetManager/AssetManager.h"
#include "BinaryStream.h"
#include "Physics.h"
#include "PhysicsUtils.h"
#include "Scene/Scene.h"

namespace Iris {

	BoxPhysicsShape::BoxPhysicsShape(Entity entity, float totalBodyMass)
		: PhysicsShape(PhysicsShapeType::Box, entity)
	{
		Ref<Scene> scene = Scene::GetScene(entity.GetSceneUUID());

		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);
		const BoxColliderComponent& boxColliderComp = entity.GetComponent<BoxColliderComponent>();

		glm::vec3 halfColliderSize = glm::abs(worldTransform.Scale * boxColliderComp.HalfSize);
		float volume = halfColliderSize.x * 2.0f * halfColliderSize.y * 2.0f * halfColliderSize.z * 2.0f;

		m_Material = JoltMaterial::CreateFromColliderMaterial(boxColliderComp.Material);

		m_Settings = new JPH::BoxShapeSettings(Utils::ToJoltVec3(halfColliderSize), 0.01f);
		m_Settings->mDensity = totalBodyMass / volume;
		m_Settings->mMaterial = m_Material;

		JPH::RotatedTranslatedShapeSettings offsetShapeSettings(Utils::ToJoltVec3(worldTransform.Scale * boxColliderComp.Offset), JPH::Quat::sIdentity(), m_Settings);

		JPH::ShapeSettings::ShapeResult result = offsetShapeSettings.Create();
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create BoxPhysicsShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::RotatedTranslatedShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void BoxPhysicsShape::SetMaterial(const ColliderMaterial& material)
	{
		m_Material->SetFriction(material.Friction);
		m_Material->SetRestitution(material.Restitution);
	}

	SpherePhysicsShape::SpherePhysicsShape(Entity entity, float totalBodyMass)
		: PhysicsShape(PhysicsShapeType::Sphere, entity)
	{
		Ref<Scene> scene = Scene::GetScene(entity.GetSceneUUID());

		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);
		const SphereColliderComponent& sphereColliderComp = entity.GetComponent<SphereColliderComponent>();

		float largestComponent = glm::abs(glm::max(worldTransform.Scale.x, glm::max(worldTransform.Scale.y, worldTransform.Scale.z)));
		float scaledRadius = largestComponent * sphereColliderComp.Radius;
		float volume = (4.0f / 3.0f) * glm::pi<float>() * (scaledRadius * scaledRadius * scaledRadius);

		m_Material = JoltMaterial::CreateFromColliderMaterial(sphereColliderComp.Material);

		m_Settings = new JPH::SphereShapeSettings(scaledRadius);
		m_Settings->mDensity = totalBodyMass / volume;
		m_Settings->mMaterial = m_Material;

		JPH::RotatedTranslatedShapeSettings offsetShapeSettings(Utils::ToJoltVec3(worldTransform.Scale * sphereColliderComp.Offset), JPH::Quat::sIdentity(), m_Settings);

		JPH::ShapeSettings::ShapeResult result = offsetShapeSettings.Create();
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create SpherePhysicsShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::RotatedTranslatedShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void SpherePhysicsShape::SetMaterial(const ColliderMaterial& material)
	{
		m_Material->SetFriction(material.Friction);
		m_Material->SetRestitution(material.Restitution);
	}

	CylinderPhysicsShape::CylinderPhysicsShape(Entity entity, float totalBodyMass)
		: PhysicsShape(PhysicsShapeType::Cylinder, entity)
	{
		Ref<Scene> scene = Scene::GetScene(entity.GetSceneUUID());

		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);
		const CylinderColliderComponent& cylinderColliderComp = entity.GetComponent<CylinderColliderComponent>();

		float largestRadius = glm::abs(glm::max(worldTransform.Scale.x, worldTransform.Scale.z));
		float scaledRadius = cylinderColliderComp.Radius * largestRadius;
		float scaledHalfHeight = glm::abs(cylinderColliderComp.HalfHeight * worldTransform.Scale.y);
		float volume = glm::pi<float>() * (scaledRadius * scaledRadius) * (scaledHalfHeight * 2.0f);

		m_Material = JoltMaterial::CreateFromColliderMaterial(cylinderColliderComp.Material);

		m_Settings = new JPH::CylinderShapeSettings(scaledHalfHeight, scaledRadius);
		m_Settings->mDensity = totalBodyMass / volume;
		m_Settings->mMaterial = m_Material;

		JPH::RotatedTranslatedShapeSettings offsetShapeSettings(Utils::ToJoltVec3(worldTransform.Scale * cylinderColliderComp.Offset), JPH::Quat::sIdentity(), m_Settings);

		JPH::ShapeSettings::ShapeResult result = offsetShapeSettings.Create();
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create CylinderPhysicsShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::RotatedTranslatedShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void CylinderPhysicsShape::SetMaterial(const ColliderMaterial& material)
	{
		m_Material->SetFriction(material.Friction);
		m_Material->SetRestitution(material.Restitution);
	}

	CapsulePhysicsShape::CapsulePhysicsShape(Entity entity, float totalBodyMass)
		: PhysicsShape(PhysicsShapeType::Capsule, entity)
	{
		Ref<Scene> scene = Scene::GetScene(entity.GetSceneUUID());

		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);
		const CapsuleColliderComponent& capsuleColliderComp = entity.GetComponent<CapsuleColliderComponent>();

		float largestRadius = glm::abs(glm::max(worldTransform.Scale.x, worldTransform.Scale.z));
		float scaledRadius = capsuleColliderComp.Radius * largestRadius;
		float scaledHalfHeight = glm::abs(capsuleColliderComp.HalfHeight * worldTransform.Scale.y);
		float volume = glm::pi<float>() * (scaledRadius * scaledRadius) * ((4.0f / 3.0f) * scaledRadius + (scaledHalfHeight * 2.0f));

		m_Material = JoltMaterial::CreateFromColliderMaterial(capsuleColliderComp.Material);

		m_Settings = new JPH::CapsuleShapeSettings(scaledHalfHeight, scaledRadius);
		m_Settings->mDensity = totalBodyMass / volume;
		m_Settings->mMaterial = m_Material;

		JPH::RotatedTranslatedShapeSettings offsetShapeSettings(Utils::ToJoltVec3(worldTransform.Scale * capsuleColliderComp.Offset), JPH::Quat::sIdentity(), m_Settings);

		JPH::ShapeSettings::ShapeResult result = offsetShapeSettings.Create();
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create CapsulePhysicsShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::RotatedTranslatedShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void CapsulePhysicsShape::SetMaterial(const ColliderMaterial& material)
	{
		m_Material->SetFriction(material.Friction);
		m_Material->SetRestitution(material.Restitution);
	}

	void MutableCompoundShape::SetMaterial(const ColliderMaterial& material)
	{
		// NOTE: Nothing to do here since each sub-collider in the CompoundCollider has its own material properties
	}

	void MutableCompoundShape::Create()
	{
		JPH::ShapeSettings::ShapeResult result = m_Settings.Create();
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create MutableCompoundShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::MutableCompoundShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void MutableCompoundShape::AddShape(Ref<PhysicsShape> shape)
	{
		Ref<Scene> scene = Scene::GetScene(m_Entity.GetSceneUUID());
		const TransformComponent& rigidBodyTransform = scene->GetWorldSpaceTransform(m_Entity);
		const TransformComponent& shapeTransform = shape->GetEntity().Transform();

		JPH::Shape* nativeShape = reinterpret_cast<JPH::Shape*>(shape->GetNativeShape());

		JPH::Vec3 translation = JPH::Vec3::sZero();
		JPH::Quat rotation = JPH::Quat::sIdentity();

		if (m_Entity != shape->GetEntity())
		{
			translation = Utils::ToJoltVec3(shapeTransform.Translation * rigidBodyTransform.Scale);
			rotation = Utils::ToJoltQuat(shapeTransform.GetRotation());
		}

		m_Settings.AddShape(translation, rotation, nativeShape);
	}

	void MutableCompoundShape::RemoveShape(Ref<PhysicsShape> shape)
	{
		// TODO: See how we can do this...
	}

	void ImmutableCompoundShape::SetMaterial(const ColliderMaterial& material)
	{
		// NOTE: Nothing to do here since each sub-collider in the CompoundCollider has its own material properties
	}

	void ImmutableCompoundShape::Create()
	{
		JPH::ShapeSettings::ShapeResult result = m_Settings.Create(*PhysicsSystem::GetTemporariesAllocator());
		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create ImmutableCompoundShape! Error: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::StaticCompoundShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void ImmutableCompoundShape::AddShape(Ref<PhysicsShape> shape)
	{
		Ref<Scene> scene = Scene::GetScene(m_Entity.GetSceneUUID());
		const TransformComponent& rigidBodyTransform = scene->GetWorldSpaceTransform(m_Entity);
		const TransformComponent& shapeTransform = shape->GetEntity().Transform();

		JPH::Shape* nativeShape = reinterpret_cast<JPH::Shape*>(shape->GetNativeShape());

		JPH::Vec3 translation = JPH::Vec3::sZero();
		JPH::Quat rotation = JPH::Quat::sIdentity();

		if (m_Entity != shape->GetEntity())
		{
			translation = Utils::ToJoltVec3(shapeTransform.Translation * rigidBodyTransform.Scale);
			rotation = Utils::ToJoltQuat(shapeTransform.GetRotation());
		}

		m_Settings.AddShape(translation, rotation, nativeShape);
	}

	void ImmutableCompoundShape::RemoveShape(Ref<PhysicsShape> shape)
	{
		IR_CORE_ERROR_TAG("Physics", "Can not remove a shape from an ImmutableCompoundShape!");
		IR_VERIFY(false, "Cannot remove a shape from an ImmutableCompoundShape");
	}

	ConvexMeshShape::ConvexMeshShape(Entity entity, float totalBodyMass)
		: PhysicsShape(PhysicsShapeType::ConvexMesh, entity)
	{
		MeshColliderComponent& component = entity.GetComponent<MeshColliderComponent>();

		Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);
		IR_VERIFY(colliderAsset);

		auto scene = Scene::GetScene(entity.GetSceneUUID());
		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);

		const CachedColliderData& colliderData = PhysicsSystem::GetMesheColliderCache()->GetMeshData(colliderAsset);
		const auto& meshData = colliderData.SimpleColliderData;
		IR_ASSERT(meshData.SubMeshes.size() > component.SubMeshIndex);

		const SubMeshColliderData& submesh = meshData.SubMeshes[component.SubMeshIndex];
		glm::vec3 submeshTranslation, submeshScale;
		glm::quat submeshRotation;
		Math::DecomposeTransform(submesh.Transform, submeshTranslation, submeshRotation, submeshScale);

		JoltBinaryStreamReader binaryReader(submesh.ColliderData);
		JPH::Shape::ShapeResult result = JPH::Shape::sRestoreFromBinaryState(binaryReader);

		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to construct ConvexMesh: {}", result.GetError());
			return;
		}

		JPH::Ref<JPH::ConvexHullShape> convexShape = Utils::CastJoltRef<JPH::ConvexHullShape>(result.Get());
		convexShape->SetDensity(totalBodyMass / convexShape->GetVolume());
		convexShape->SetMaterial(JoltMaterial::CreateFromColliderMaterial(component.Material));

		m_Shape = new JPH::ScaledShape(convexShape, Utils::ToJoltVec3(submeshScale * worldTransform.Scale));
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void ConvexMeshShape::SetMaterial(const ColliderMaterial& material)
	{
	}

	// NOTE: Having materials for triangle shapes is a bit tricky in Jolt...
	TriangleMeshShape::TriangleMeshShape(Entity entity)
		: PhysicsShape(PhysicsShapeType::TriangleMesh, entity)
	{
		MeshColliderComponent& component = entity.GetComponent<MeshColliderComponent>();

		Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);
		IR_VERIFY(colliderAsset);

		auto scene = Scene::GetScene(entity.GetSceneUUID());
		TransformComponent worldTransform = scene->GetWorldSpaceTransform(entity);

		const CachedColliderData& colliderData = PhysicsSystem::GetMesheColliderCache()->GetMeshData(colliderAsset);
		const auto& meshData = colliderData.ComplexColliderData;

		JPH::StaticCompoundShapeSettings compoundShapeSettings;
		for (const SubMeshColliderData& submeshData : meshData.SubMeshes)
		{
			glm::vec3 submeshTranslation, submeshScale;
			glm::quat submeshRotation;
			Math::DecomposeTransform(submeshData.Transform, submeshTranslation, submeshRotation, submeshScale);

			JoltBinaryStreamReader binaryReader(submeshData.ColliderData);
			JPH::Shape::ShapeResult result = JPH::Shape::sRestoreFromBinaryState(binaryReader);

			if (result.HasError())
			{
				IR_CORE_ERROR_TAG("Physics", "Failed to construct ConvexMesh!");
				return;
			}

			compoundShapeSettings.AddShape(Utils::ToJoltVec3(submeshTranslation), Utils::ToJoltQuat(submeshRotation), new JPH::ScaledShape(result.Get(), Utils::ToJoltVec3(submeshScale * worldTransform.Scale)));
		}

		JPH::Shape::ShapeResult result = compoundShapeSettings.Create();

		if (result.HasError())
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to construct ConvexMesh: {}", result.GetError());
			return;
		}

		m_Shape = Utils::CastJoltRef<JPH::StaticCompoundShape>(result.Get());
		m_Shape->SetUserData(reinterpret_cast<JPH::uint64>(this));
	}

	void TriangleMeshShape::SetMaterial(const ColliderMaterial& material)
	{
	}

}