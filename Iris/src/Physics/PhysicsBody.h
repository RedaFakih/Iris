#pragma once

#include "PhysicsShapes.h"
#include "PhysicsTypes.h"
#include "Scene/Scene.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>

namespace Iris {

	class PhysicsBody : public RefCountedObject
	{
	public:
		PhysicsBody(JPH::BodyInterface& bodyInterface, Entity entity);
		~PhysicsBody();

		[[nodiscard]] inline static Ref<PhysicsBody> Create(JPH::BodyInterface& bodyInterface, Entity entity)
		{
			return CreateRef<PhysicsBody>(bodyInterface, entity);
		}

		void SetCollisionLayer(uint32_t layerId);

		void MoveKinematic(const glm::vec3& targetPosition, const glm::quat& targetRotation, float timestep);
		void Rotate(const glm::vec3& inRotationTimeDeltaTime);

		void SetGravityEnabled(bool enabled);
		void AddForce(const glm::vec3& force, PhysicsForceMode forceMode = PhysicsForceMode::Force, bool forceAwake = true);
		void AddForce(const glm::vec3& force, const glm::vec3& location, PhysicsForceMode forceMode = PhysicsForceMode::Force, bool forceAwake = true);
		void AddTorque(const glm::vec3& torque, bool forceAwake = true);
		void AddRadialImpulse(const glm::vec3& origin, float radius, float strength, PhysicsFallOffMode fallOffMode, bool velocityChange);

		void ChangeTriggerState(bool isTrigger);

		void SetMass(float mass);
		float GetMass() const;

		void SetLinearDrag(float inLinearDrag);
		float GetLinearDrag() const;

		void SetAngularDrag(float inAngularDrag);
		float GetAngularDrag() const;

		void SetLinearVelocity(const glm::vec3& inVelocity);
		glm::vec3 GetLinearVelocity() const;

		void SetAngularVelocity(const glm::vec3& inVelocity);
		glm::vec3 GetAngularVelocity() const;

		void SetMaxLinearVelocity(float maxLinearVelocity);
		float GetMaxLinearVelocity() const;

		void SetMaxAngularVelocity(float maxAngularVelocity);
		float GetMaxAngularVelocity() const;

		void SetSleepState(bool isSleeping);
		bool IsSleeping() const;

		void SetCollisionDetectionMode(PhysicsCollisionDetectionType collisionDetectionMode);

		void SetTranslation(const glm::vec3& translation);
		glm::vec3 GetTranslation() const;

		void SetRotation(const glm::quat& rotation);
		glm::quat GetRotation() const;

		void SetAxisLock(PhysicsActorAxis axis, bool locked, bool forceAwake);
		bool IsAxisLocked(PhysicsActorAxis axis) const;
		PhysicsActorAxis GetLockedAxes() const { return m_LockedAxes; }
		bool IsAllRotationLocked() const { return IsAxisLocked(PhysicsActorAxis::RotationX) && IsAxisLocked(PhysicsActorAxis::RotationY) && IsAxisLocked(PhysicsActorAxis::RotationZ); }

		bool IsStatic() const;
		bool IsDynamic() const;
		bool IsKinematic() const;
		bool IsTrigger() const;

		JPH::BodyID GetBodyID() const { return m_BodyID; }
		Entity GetEntity() const { return m_Entity; }

		const std::vector<Ref<PhysicsShape>>& GetPhysicsShapes(PhysicsShapeType type) const
		{
			if (m_Shapes.contains(type))
				return m_Shapes.at(type);

			return {};
		}

		uint32_t GetPhysicsShapeCount(PhysicsShapeType type) const
		{
			if (m_Shapes.contains(type))
				return static_cast<uint32_t>(m_Shapes.at(type).size());

			return 0;
		}

		Ref<PhysicsShape> GetPhysicsShape(PhysicsShapeType type, uint32_t index) const
		{
			if (!m_Shapes.contains(type) || index >= static_cast<uint32_t>(m_Shapes.at(type).size()))
				return nullptr;

			return m_Shapes.at(type).at(index);
		}

	private:
		void OnAxisLockUpdated(bool forceWake);

		void CreateStaticBody(JPH::BodyInterface& bodyInterface);
		void CreateDynamicBody(JPH::BodyInterface& bodyInterface);

		void CreateCollisionShapeForEntity(Entity entity, bool ignoreCompoundShapes = false);

		void CreateAxisLockConstraint(JPH::Body& body);

		void Release();

	private:
		Entity m_Entity;
		std::unordered_map<PhysicsShapeType, std::vector<Ref<PhysicsShape>>> m_Shapes;
		PhysicsActorAxis m_LockedAxes = PhysicsActorAxis::None;

		JPH::BodyID m_BodyID;
		// Calling it `Old` since maybe the user wants the body to change from static to dynamic during runtime...
		JPH::EMotionType m_OldMotionType = JPH::EMotionType::Static;

		glm::vec3 m_KinematicTargetPosition = { 0.0f, 0.0f, 0.0f };
		glm::quat m_KinematicTargetRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		float m_KinematicTargetTime = 0.0f;

		JPH::SixDOFConstraint* m_AxisLockConstraint = nullptr;

		friend class PhysicsScene;

	};

}