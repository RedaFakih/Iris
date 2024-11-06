#pragma once

#include "ContactListener.h"
#include "LayerInterface.h"
#include "PhysicsBody.h"
#include "PhysicsTypes.h"
#include "PhysicsUtils.h"
#include "Scene/Scene.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace Iris {

	/*
	 * Just like a normal scene holds all the info for the renderer and everything else, a PhysicsScene holds all the info and data required for the physics to work.
	 * That includes the stuff like all the bodies existing in the scene and the collision shapes for each body, etc...
	 */

	class PhysicsScene : public RefCountedObject
	{
	public:
		PhysicsScene(const Ref<Scene>& scene);
		~PhysicsScene();

		[[nodiscard]] inline static Ref<PhysicsScene> Create(const Ref<Scene>& scene)
		{
			return CreateRef<PhysicsScene>(scene);
		}

		void Destroy();

		void Simulate(float ts);

		Ref<PhysicsBody> CreateBody(Entity entity, PhysicsBodyAddType addType = PhysicsBodyAddType::AddImmediate);
		void DestroyBody(Entity entity);

		void SetBodyType(Entity entity, PhysicsBodyType bodyType);

		inline void MarkForSynchronization(Ref<PhysicsBody> body)
		{
			m_BodiesScheduledForSync.push_back(body);
		}

		Ref<PhysicsBody> GetExistingPhysicsBodyByID(UUID uuid) const { return m_RigidBodies.contains(uuid) ? m_RigidBodies.at(uuid) : nullptr; }
		Ref<PhysicsBody> GetExistingPhysicsBody(Entity entity) const { return GetExistingPhysicsBodyByID(entity.GetUUID()); }

		// static functions
		static JPH::BodyInterface& GetBodyInterface(bool shouldLock = true);
		static const JPH::BodyLockInterface& GetBodyLockInterface(bool shouldLock = true);
		static JPH::PhysicsSystem& GetJoltSystem();

		inline Ref<Scene> GetContextScene() { return m_ContextScene; }
		inline const Ref<Scene>& GetContextScene() const { return m_ContextScene; }

		inline void SetGravity(const glm::vec3& gravityXYZ) { m_JoltSystem->SetGravity(Utils::ToJoltVec3(gravityXYZ)); }
		inline glm::vec3 GetGravity() const { return Utils::FromJoltVec3(m_JoltSystem->GetGravity()); }

	private:
		void SubStepStrategy(float ts);
		void PreSimulate(float ts);
		void PostSimulate();

		void CreateRigidBodies();

		// explicit body synchronization is to make sure that a certain change happens in the TransformComponent
		void SynchronizePendingBodyTransforms();
		void SynchronizeBodyTransform(WeakRef<PhysicsBody> body);

	private:
		Ref<Scene> m_ContextScene;

		Scope<JPH::PhysicsSystem> m_JoltSystem;
		Scope<JoltLayerInterface> m_JoltLayerInterface;
		Scope<JoltContactListener> m_JoltContactListener;

		std::unordered_map<UUID, Ref<PhysicsBody>> m_RigidBodies;

		std::vector<WeakRef<PhysicsBody>> m_BodiesScheduledForSync;

		float m_Accumulator = 0.0f;
		uint32_t m_CollisionSteps = 1;

		inline constexpr static uint32_t s_BroadPhaseOptimizationThreashold = 500;

	};

}