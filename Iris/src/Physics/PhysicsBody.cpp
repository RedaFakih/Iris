#include "IrisPCH.h"
#include "PhysicsBody.h"

#include "PhysicsUtils.h"
#include "PhysicsScene.h"
#include "PhysicsLayer.h"

#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <magic_enum.hpp>
using namespace magic_enum::bitwise_operators;

namespace Iris {

	PhysicsBody::PhysicsBody(JPH::BodyInterface& bodyInterface, Entity entity)
		: m_Entity(entity)
	{
		const RigidBodyComponent& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();
		m_LockedAxes = rigidBodyComponent.LockedAxes;

		switch (rigidBodyComponent.BodyType)
		{
			case PhysicsBodyType::Static:
			{
				CreateStaticBody(bodyInterface);
				break;
			}
			case PhysicsBodyType::Dynamic:
			case PhysicsBodyType::Kinematic:
			{
				CreateDynamicBody(bodyInterface);
				break;
			}
		}

		m_OldMotionType = Utils::ToJoltMotionType(rigidBodyComponent.BodyType);
	}

	PhysicsBody::~PhysicsBody()
	{
		// TODO: Maybe we can add Release here?
	}

	void PhysicsBody::SetCollisionLayer(uint32_t layerId)
	{
		PhysicsScene::GetBodyInterface().SetObjectLayer(m_BodyID, static_cast<JPH::ObjectLayer>(layerId));
	}

	void PhysicsBody::MoveKinematic(const glm::vec3& targetPosition, const glm::quat& targetRotation, float timestep)
	{
		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		if (body.GetMotionType() != JPH::EMotionType::Kinematic)
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to MoveKinematic on non-kinematic body!");
			return;
		}

		body.MoveKinematic(Utils::ToJoltVec3(targetPosition), Utils::ToJoltQuat(targetRotation), timestep);
	}

	void PhysicsBody::Rotate(const glm::vec3& inRotationTimeDeltaTime)
	{
		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		if (body.GetMotionType() != JPH::EMotionType::Kinematic)
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to MoveKinematic on non-kinematic body!");
			return;
		}

		body.AddRotationStep(Utils::ToJoltVec3(inRotationTimeDeltaTime));
	}

	void PhysicsBody::SetGravityEnabled(bool enabled)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetGravityEnabled on non-dynamic body");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		bodyLock.GetBody().GetMotionProperties()->SetGravityFactor(enabled ? 1.0f : 0.0f);
	}

	void PhysicsBody::AddForce(const glm::vec3& force, PhysicsForceMode forceMode, bool forceAwake)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to AddForce on non-dynamic body");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		switch (forceMode)
		{
			case PhysicsForceMode::Force:
			{
				body.AddForce(Utils::ToJoltVec3(force));
				break;
			}
			case PhysicsForceMode::Impulse:
			{
				body.AddImpulse(Utils::ToJoltVec3(force));
				break;
			}
			case PhysicsForceMode::VelocityChange:
			{
				body.SetLinearVelocityClamped(body.GetLinearVelocity() + Utils::ToJoltVec3(force));

				// Do not try to activate the body if the velocity is near zero
				if (body.GetLinearVelocity().IsNearZero())
					return;

				break;
			}
			case PhysicsForceMode::Acceleration:
			{
				// Acceleration can be applied by adding it as a force divided by the inverse mass, since that negates
				// the mass inclusion in the integration steps (which is what differentiates a force being applied vs. an acceleration)
				body.AddForce(Utils::ToJoltVec3(force) / body.GetMotionProperties()->GetInverseMass());
				break;
			}
		}

		// Use the non-locking version of the bodyInterface. We've already grabbed a lock for the body
		if (body.IsInBroadPhase() && !body.IsActive())
			PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
	}

	void PhysicsBody::AddForce(const glm::vec3& force, const glm::vec3& location, PhysicsForceMode forceMode, bool forceAwake)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to AddForce on non-dynamic body");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		switch (forceMode)
		{
			case PhysicsForceMode::Force:
			{
				body.AddForce(Utils::ToJoltVec3(force), Utils::ToJoltVec3(location));
				break;
			}
			case PhysicsForceMode::Impulse:
			{
				body.AddImpulse(Utils::ToJoltVec3(force), Utils::ToJoltVec3(location));
				break;
			}
			case PhysicsForceMode::VelocityChange:
			{
				// Do not think we can change the velocity at a specific point
				body.SetLinearVelocityClamped(body.GetLinearVelocity() + Utils::ToJoltVec3(force));

				// Do not try to activate the body if the velocity is near zero
				if (body.GetLinearVelocity().IsNearZero())
					return;

				break;
			}
			case PhysicsForceMode::Acceleration:
			{
				// Acceleration can be applied by adding it as a force divided by the inverse mass, since that negates
				// the mass inclusion in the integration steps (which is what differentiates a force being applied vs. an acceleration)
				body.AddForce(Utils::ToJoltVec3(force) / body.GetMotionProperties()->GetInverseMass(), Utils::ToJoltVec3(location));
				break;
			}
		}

		// Use the non-locking version of the bodyInterface. We've already grabbed a lock for the body
		if (body.IsInBroadPhase() && !body.IsActive())
			PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
	}

	void PhysicsBody::AddTorque(const glm::vec3& torque, bool forceAwake)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to AddTorque on non-dynamic body");
			return;
		}

		PhysicsScene::GetBodyInterface().AddTorque(m_BodyID, Utils::ToJoltVec3(torque));
	}

	void PhysicsBody::AddRadialImpulse(const glm::vec3& origin, float radius, float strength, PhysicsFallOffMode fallOffMode, bool velocityChange)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to AddRadialImpulse on non-dynamic body");
			return;
		}

		JPH::BodyLockRead readLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(readLock.Succeeded());

		const JPH::Body& readBody = readLock.GetBody();
		glm::vec3 centerOfMassPosition = Utils::FromJoltVec3(readBody.GetCenterOfMassPosition());
		readLock.ReleaseLock();

		glm::vec3 direction = centerOfMassPosition - origin;
		
		float distance = glm::length(direction);
		if (distance > radius)
			return;

		direction = glm::normalize(direction);

		float impulseMagnitude = strength;

		if (fallOffMode == PhysicsFallOffMode::Linear)
			impulseMagnitude *= (1.0f - (distance / radius));

		glm::vec3 impulse = direction * impulseMagnitude;

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& writeBody = bodyLock.GetBody();

		if (velocityChange)
		{
			JPH::Vec3 linearVelocity = writeBody.GetLinearVelocity() + Utils::ToJoltVec3(impulse);
			writeBody.SetLinearVelocityClamped(linearVelocity);

			// return so that we do not activate the body
			if (linearVelocity.IsNearZero())
				return;
		}
		else
		{
			writeBody.AddImpulse(Utils::ToJoltVec3(impulse));
		}

		if (!writeBody.IsActive())
			PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
	}

	void PhysicsBody::ChangeTriggerState(bool isTrigger)
	{
		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		bodyLock.GetBody().SetIsSensor(isTrigger);
	}

	void PhysicsBody::SetMass(float mass)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a mass!");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& body = bodyLock.GetBody();

		JPH::MassProperties massProperties = body.GetShape()->GetMassProperties();
		massProperties.ScaleToMass(mass);
		massProperties.mInertia(3, 3) = 1.0f;
		body.GetMotionProperties()->SetMassProperties(JPH::EAllowedDOFs::All, massProperties);
	}

	float PhysicsBody::GetMass() const
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a mass!");
			return 0.0f;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return 1.0f / bodyLock.GetBody().GetMotionProperties()->GetInverseMass(); // Invert the mass twice to cancel the inversion
	}

	void PhysicsBody::SetLinearDrag(float inLinearDrag)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a linear drag");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		bodyLock.GetBody().GetMotionProperties()->SetLinearDamping(inLinearDrag);
	}

	float PhysicsBody::GetLinearDrag() const
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a linear drag!");
			return 0.0f;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return bodyLock.GetBody().GetMotionProperties()->GetLinearDamping();
	}

	void PhysicsBody::SetAngularDrag(float inAngularDrag)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a angular drag");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		bodyLock.GetBody().GetMotionProperties()->SetAngularDamping(inAngularDrag);
	}

	float PhysicsBody::GetAngularDrag() const
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "StaticBody does not have a angular drag!");
			return 0.0f;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return bodyLock.GetBody().GetMotionProperties()->GetAngularDamping();
	}

	void PhysicsBody::SetLinearVelocity(const glm::vec3& inVelocity)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetLinearVelocity on a non-dynamic body! Move this body using the entity's TransformComponent");
			return;
		}

		PhysicsScene::GetBodyInterface().SetLinearVelocity(m_BodyID, Utils::ToJoltVec3(inVelocity));
	}

	glm::vec3 PhysicsBody::GetLinearVelocity() const
	{
		if (IsStatic())
		{
			return glm::vec3(0.0f);
		}

		return Utils::FromJoltVec3(PhysicsScene::GetBodyInterface().GetLinearVelocity(m_BodyID));
	}

	void PhysicsBody::SetAngularVelocity(const glm::vec3& inVelocity)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetAngularVelocity on a non-dynamic body! Move this body using the entity's TransformComponent");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& body = bodyLock.GetBody();

		body.SetAngularVelocity(Utils::ToJoltVec3(inVelocity));

		if (!body.IsActive() && !Utils::ToJoltVec3(inVelocity).IsNearZero() && body.IsInBroadPhase())
			PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
	}

	glm::vec3 PhysicsBody::GetAngularVelocity() const
	{
		if (IsStatic())
		{
			return glm::vec3(0.0f);
		}

		return Utils::FromJoltVec3(PhysicsScene::GetBodyInterface().GetAngularVelocity(m_BodyID));
	}

	void PhysicsBody::SetMaxLinearVelocity(float maxLinearVelocity)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetMaxLinearVelocity on a non-dynamic body!");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		bodyLock.GetBody().GetMotionProperties()->SetMaxLinearVelocity(maxLinearVelocity);
	}

	float PhysicsBody::GetMaxLinearVelocity() const
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to GetMaxLinearVelocity on a non-dynamic body!");
			return 0.0f;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return bodyLock.GetBody().GetMotionProperties()->GetMaxLinearVelocity();
	}

	void PhysicsBody::SetMaxAngularVelocity(float maxAngularVelocity)
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetMaxAngularVelocity on a non-dynamic body!");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		bodyLock.GetBody().GetMotionProperties()->SetMaxAngularVelocity(maxAngularVelocity);
	}

	float PhysicsBody::GetMaxAngularVelocity() const
	{
		if (!IsDynamic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to GetMaxAngularVelocity on a non-dynamic body!");
			return 0.0f;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return bodyLock.GetBody().GetMotionProperties()->GetMaxAngularVelocity();
	}

	void PhysicsBody::SetSleepState(bool isSleeping)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetSleepState on a static body");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		if (isSleeping)
		{
			PhysicsScene::GetBodyInterface(false).DeactivateBody(m_BodyID);
		}
		else
		{
			PhysicsScene::GetBodyInterface(false).ActivateBody(m_BodyID);
		}
	}

	bool PhysicsBody::IsSleeping() const
	{
		if (IsStatic())
		{
			return false;
		}

		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return !bodyLock.GetBody().IsActive();
	}

	void PhysicsBody::SetCollisionDetectionMode(PhysicsCollisionDetectionType collisionDetectionMode)
	{
		PhysicsScene::GetBodyInterface().SetMotionQuality(m_BodyID, Utils::ToJoltMotionQuality(collisionDetectionMode));
	}

	void PhysicsBody::SetTranslation(const glm::vec3& translation)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetTranslation on a static body!");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& body = bodyLock.GetBody();
		
		PhysicsScene::GetBodyInterface(false).SetPosition(m_BodyID, Utils::ToJoltVec3(translation), JPH::EActivation::DontActivate);

		// NOTE: Mark the body for synchronization to make sure that this change happens in the transform component
		Ref<Scene> entityScene = Scene::GetScene(m_Entity.GetSceneUUID());
		entityScene->GetPhysicsScene()->MarkForSynchronization(this);
	}

	glm::vec3 PhysicsBody::GetTranslation() const
	{
		return Utils::FromJoltVec3(PhysicsScene::GetBodyInterface().GetPosition(m_BodyID));
	}

	void PhysicsBody::SetRotation(const glm::quat& rotation)
	{
		if (IsStatic())
		{
			IR_CORE_ERROR_TAG("Physics", "Invalid call to SetRotation on a static body!");
			return;
		}

		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& body = bodyLock.GetBody();

		PhysicsScene::GetBodyInterface(false).SetRotation(m_BodyID, Utils::ToJoltQuat(rotation), JPH::EActivation::DontActivate);

		// NOTE: Mark the body for synchronization to make sure that this change happens in the transform component
		Ref<Scene> entityScene = Scene::GetScene(m_Entity.GetSceneUUID());
		entityScene->GetPhysicsScene()->MarkForSynchronization(this);
	}

	glm::quat PhysicsBody::GetRotation() const
	{
		return Utils::FromJoltQuat(PhysicsScene::GetBodyInterface().GetRotation(m_BodyID));
	}

	void PhysicsBody::SetAxisLock(PhysicsActorAxis axis, bool locked, bool forceAwake)
	{
		if (locked)
			m_LockedAxes |= axis;
		else
			m_LockedAxes &= ~axis;

		OnAxisLockUpdated(forceAwake);
	}

	bool PhysicsBody::IsAxisLocked(PhysicsActorAxis axis) const
	{
		return (m_LockedAxes & axis) != PhysicsActorAxis::None;
	}

	bool PhysicsBody::IsStatic() const
	{
		return PhysicsScene::GetBodyInterface().GetMotionType(m_BodyID) == JPH::EMotionType::Static;
	}

	bool PhysicsBody::IsDynamic() const
	{
		return PhysicsScene::GetBodyInterface().GetMotionType(m_BodyID) == JPH::EMotionType::Dynamic;
	}

	bool PhysicsBody::IsKinematic() const
	{
		return PhysicsScene::GetBodyInterface().GetMotionType(m_BodyID) == JPH::EMotionType::Kinematic;
	}

	bool PhysicsBody::IsTrigger() const
	{
		JPH::BodyLockRead bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		return bodyLock.GetBody().IsSensor();
	}

	void PhysicsBody::OnAxisLockUpdated(bool forceWake)
	{
		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		if (m_AxisLockConstraint)
		{
			PhysicsScene::GetJoltSystem().RemoveConstraint(m_AxisLockConstraint);
			m_AxisLockConstraint = nullptr;
		}

		// Do not recreate if we have no locked axes
		if (m_LockedAxes != PhysicsActorAxis::None)
			CreateAxisLockConstraint(bodyLock.GetBody());
	}

	void PhysicsBody::CreateStaticBody(JPH::BodyInterface& bodyInterface)
	{
		Ref<Scene> scene = Scene::GetScene(m_Entity.GetSceneUUID());
		IR_VERIFY(scene, "There should one active scene at least!");

		const TransformComponent& worldTransform = scene->GetWorldSpaceTransform(m_Entity);
		const RigidBodyComponent& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

		CreateCollisionShapeForEntity(m_Entity);

		if (m_Shapes.empty())
			return;

		JPH::Shape* shape = nullptr;

		// We check if we have a Mutable/Immutable CompoundCollider first since we can't just loop through the map since that does not makes sense for what we need
		// Since the ShapeType is set explicitly so we can not just use any generic type in the map
		if (m_Shapes.contains(PhysicsShapeType::CompoundShape))
		{
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.at(PhysicsShapeType::CompoundShape)[0]->GetNativeShape());
		}
		else if (m_Shapes.contains(PhysicsShapeType::MutableCompoundShape))
		{
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.at(PhysicsShapeType::MutableCompoundShape)[0]->GetNativeShape());
		}
		else
		{
			// Otherwise we just get the first element in the map which should be the shape of the collider (Box, Sphere, ...)
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.begin()->second[0]->GetNativeShape());
		}
		
		if (shape == nullptr)
		{
			IR_CORE_WARN_TAG("Physics", "Failed to create static PhysicsBody since there was no collision shape provided!");
			m_Shapes.clear();
			return;
		}

		JPH::BodyCreationSettings bodySettings(
			shape,
			Utils::ToJoltVec3(worldTransform.Translation),
			Utils::ToJoltQuat(glm::normalize(worldTransform.GetRotation())),
			JPH::EMotionType::Static,
			static_cast<JPH::ObjectLayer>(rigidBodyComponent.LayerID)
		);
		bodySettings.mIsSensor = rigidBodyComponent.IsTrigger;
		bodySettings.mAllowDynamicOrKinematic = rigidBodyComponent.EnableDynamicTypeChange;
		bodySettings.mAllowSleeping = false;

		JPH::Body* body = bodyInterface.CreateBody(bodySettings);
	
		if (!body)
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create PhysicsBody");
			return;
		}

		body->SetUserData(m_Entity.GetUUID());
		m_BodyID = body->GetID();
	}

	void PhysicsBody::CreateDynamicBody(JPH::BodyInterface& bodyInterface)
	{
		Ref<Scene> scene = Scene::GetScene(m_Entity.GetSceneUUID());
		IR_VERIFY(scene, "There should one active scene at least!");

		const TransformComponent& worldTransform = scene->GetWorldSpaceTransform(m_Entity);
		RigidBodyComponent& rigidBodyComponent = m_Entity.GetComponent<RigidBodyComponent>();

		CreateCollisionShapeForEntity(m_Entity);

		if (m_Shapes.empty())
			return;

		JPH::Shape* shape = nullptr;

		// We check if we have a Mutable/Immutable CompoundCollider first since we can't just loop through the map since that does not makes sense for what we need
		// Since the ShapeType is set explicitly so we can not just use any generic type in the map
		if (m_Shapes.contains(PhysicsShapeType::CompoundShape))
		{
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.at(PhysicsShapeType::CompoundShape)[0]->GetNativeShape());
		}
		else if (m_Shapes.contains(PhysicsShapeType::MutableCompoundShape))
		{
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.at(PhysicsShapeType::MutableCompoundShape)[0]->GetNativeShape());
		}
		else
		{
			// Otherwise we just get the first element in the map which should be the shape of the collider (Box, Sphere, ...)
			shape = reinterpret_cast<JPH::Shape*>(m_Shapes.begin()->second[0]->GetNativeShape());
		}

		if (shape == nullptr)
		{
			IR_CORE_WARN_TAG("Physics", "Failed to create static PhysicsBody since there was no collision shape provided!");
			m_Shapes.clear();
			return;
		}

		if (!PhysicsLayerManager::IsLayerValid(rigidBodyComponent.LayerID))
		{
			rigidBodyComponent.LayerID = 0;
			IR_CORE_WARN_TAG("Physics", "Entity '{}' has a RigidBodyComponent with an invalid layer set! Will use default layer instead", m_Entity.Name());
		}

		JPH::BodyCreationSettings bodySettings(
			shape,
			Utils::ToJoltVec3(worldTransform.Translation),
			Utils::ToJoltQuat(glm::normalize(worldTransform.GetRotation())),
			Utils::ToJoltMotionType(rigidBodyComponent.BodyType),
			static_cast<JPH::ObjectLayer>(rigidBodyComponent.LayerID)
		);
		bodySettings.mLinearDamping = rigidBodyComponent.LinearDrag;
		bodySettings.mAngularDamping = rigidBodyComponent.AngularDrag;
		bodySettings.mIsSensor = rigidBodyComponent.IsTrigger;
		bodySettings.mGravityFactor = rigidBodyComponent.DisableGravity ? 0.0f : 1.0f;
		bodySettings.mLinearVelocity = Utils::ToJoltVec3(rigidBodyComponent.InitialLinearVelocity);
		bodySettings.mAngularVelocity = Utils::ToJoltVec3(rigidBodyComponent.InitialAngularVelocity);
		bodySettings.mMaxLinearVelocity = rigidBodyComponent.MaxLinearVelocity;
		bodySettings.mMaxAngularVelocity = rigidBodyComponent.MaxAngularVelocity;
		bodySettings.mMotionQuality = Utils::ToJoltMotionQuality(rigidBodyComponent.CollisionDetection);
		bodySettings.mAllowSleeping = true;

		JPH::Body* body = bodyInterface.CreateBody(bodySettings);
		if (!body)
		{
			IR_CORE_ERROR_TAG("Physics", "Failed to create PhysicsBody");
			return;
		}

		body->SetUserData(m_Entity.GetUUID());
		m_BodyID = body->GetID();

		// Only create the constraint if it's actually needed
		if (rigidBodyComponent.LockedAxes != PhysicsActorAxis::None)
			CreateAxisLockConstraint(*body);
	}

	void PhysicsBody::CreateCollisionShapeForEntity(Entity entity, bool ignoreCompoundShapes)
	{
		Ref<Scene> scene = Scene::GetScene(entity.GetSceneUUID());

		/*
		 * TODO: Compound collision shape is one rigid body that is composed from multiple different collision shapes linked to each other, meaning it could be one body
		 * but that one body has a box and a sphere shape linked to it as its collision shapes, so for that to work we need to have a generic array or map
		 * of shape type to vector of collision shapes since it may also have multiple of each shape type
		 */
		if (entity.HasComponent<CompoundColliderComponent>() && !ignoreCompoundShapes)
		{
			const CompoundColliderComponent& compoundColliderComp = entity.GetComponent<CompoundColliderComponent>();

			Ref<CompoundShape> compoundShape = nullptr;

			if (compoundColliderComp.IsImmutable)
				compoundShape = ImmutableCompoundShape::Create(entity);
			else
				compoundShape = MutableCompoundShape::Create(entity);

			for (UUID colliderEntityID : compoundColliderComp.CompoundedColliderEntities)
			{
				Entity colliderEntity = scene->TryGetEntityWithUUID(colliderEntityID);

				if (!colliderEntity)
					continue;

				CreateCollisionShapeForEntity(colliderEntity, colliderEntity == m_Entity || colliderEntity == entity);
			}

			for (const auto& [shapeType, shapeStorage] : m_Shapes)
			{
				for (const auto& shape : shapeStorage)
					compoundShape->AddShape(shape);
			}

			compoundShape->Create();
			m_Shapes[compoundShape->GetType()].push_back(compoundShape);

			return;
		}

		// Just in case the user tries to play the runtime with a scene that has a CompoundColliderComponent but one of its child colliders does not
		// have a RigidBodyComponent
		if (!entity.HasComponent<RigidBodyComponent>())
		{
			IR_CORE_ERROR_TAG("Physics", "Entity `{}` does not have a RigidBodyComponent so no collision shape can be created for it!", entity.Name());
			return;
		}

		const RigidBodyComponent& rigidBodyComp = entity.GetComponent<RigidBodyComponent>();

		if (entity.HasComponent<BoxColliderComponent>())
		{
			m_Shapes[PhysicsShapeType::Box].push_back(BoxPhysicsShape::Create(entity, rigidBodyComp.Mass));
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			m_Shapes[PhysicsShapeType::Sphere].push_back(SpherePhysicsShape::Create(entity, rigidBodyComp.Mass));
		}

		if (entity.HasComponent<CylinderColliderComponent>())
		{
			m_Shapes[PhysicsShapeType::Cylinder].push_back(CylinderPhysicsShape::Create(entity, rigidBodyComp.Mass));
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			m_Shapes[PhysicsShapeType::Capsule].push_back(CapsulePhysicsShape::Create(entity, rigidBodyComp.Mass));
		}

		// TODO: Other types MeshCollider
	}

	void PhysicsBody::CreateAxisLockConstraint(JPH::Body& body)
	{
		JPH::SixDOFConstraintSettings constraintSettings;
		constraintSettings.mPosition1 = constraintSettings.mPosition2 = body.GetCenterOfMassPosition();

		if ((m_LockedAxes & PhysicsActorAxis::TranslationX) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::TranslationX);

		if ((m_LockedAxes & PhysicsActorAxis::TranslationY) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::TranslationY);

		if ((m_LockedAxes & PhysicsActorAxis::TranslationZ) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::TranslationZ);

		if ((m_LockedAxes & PhysicsActorAxis::RotationX) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::RotationX);

		if ((m_LockedAxes & PhysicsActorAxis::RotationY) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::RotationY);

		if ((m_LockedAxes & PhysicsActorAxis::RotationZ) != PhysicsActorAxis::None)
			constraintSettings.MakeFixedAxis(JPH::SixDOFConstraintSettings::RotationZ);

		m_AxisLockConstraint = reinterpret_cast<JPH::SixDOFConstraint*>(constraintSettings.Create(JPH::Body::sFixedToWorld, body));

		PhysicsScene::GetJoltSystem().AddConstraint(m_AxisLockConstraint);
	}

	void PhysicsBody::Release()
	{
		JPH::BodyLockWrite bodyLock(PhysicsScene::GetBodyLockInterface(), m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());
		JPH::Body& body = bodyLock.GetBody();

		if (m_AxisLockConstraint)
		{
			PhysicsScene::GetJoltSystem().RemoveConstraint(m_AxisLockConstraint);
			m_AxisLockConstraint = nullptr;
		}

		if (body.IsInBroadPhase())
			PhysicsScene::GetBodyInterface(false).RemoveBody(m_BodyID);
		else
			PhysicsScene::GetBodyInterface(false).DeactivateBody(m_BodyID);

		m_Shapes.clear();
		PhysicsScene::GetBodyInterface(false).DestroyBody(m_BodyID);
	}

}