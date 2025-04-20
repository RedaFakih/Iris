#include "IrisPCH.h"
#include "PhysicsScene.h"

#include "Physics.h"
#include "PhysicsLayer.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>

namespace Iris {

	/// Class to test if an object can collide with a broadphase layer. Used while finding collision pairs.
	class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		/// Returns true if an object layer should collide with a broadphase layer
		bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			const PhysicsLayer& layerData1 = PhysicsLayerManager::GetLayer(inLayer1);
			const PhysicsLayer& layerData2 = PhysicsLayerManager::GetLayer(static_cast<uint8_t>(inLayer2));

			if (layerData1.LayerID == layerData2.LayerID)
				return layerData1.CollidesWithSelf;

			return layerData1.CollidesWith & layerData2.BitValue;
		}
	};

	/// Filter class to test if two objects can collide based on their object layer. Used while finding collision pairs.
	class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		/// Returns true if two layers can collide
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
		{
			const PhysicsLayer& layerData1 = PhysicsLayerManager::GetLayer(inLayer1);
			const PhysicsLayer& layerData2 = PhysicsLayerManager::GetLayer(inLayer2);

			if (layerData1.LayerID == layerData2.LayerID)
				return layerData1.CollidesWithSelf;

			return layerData1.CollidesWith & layerData2.BitValue;
		}
	};

	// For the JPH::PhysicsSystem
	static ObjectLayerPairFilterImpl s_ObjectVsObjectLayerFilter;
	static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilter;
	// We can not have nore than one scene physics simulation happening at the same time
	static PhysicsScene* s_CurrentInstance = nullptr;

	PhysicsScene::PhysicsScene(const Ref<Scene>& scene)
		: m_ContextScene(scene)
	{
		IR_VERIFY(s_CurrentInstance == nullptr, "Shouldn't have multiple instances of a physics scene!");
		s_CurrentInstance = this;

		const PhysicsSettings& settings = PhysicsSystem::GetSettings();

		m_JoltSystem = CreateScope<JPH::PhysicsSystem>();
		m_JoltLayerInterface = CreateScope<JoltLayerInterface>();

		m_JoltSystem->Init(20240, 0, 65536, 20240, *m_JoltLayerInterface, s_ObjectVsBroadPhaseLayerFilter, s_ObjectVsObjectLayerFilter);
		m_JoltSystem->SetGravity(Utils::ToJoltVec3(settings.Gravity));

		JPH::PhysicsSettings joltSettings;
		joltSettings.mNumPositionSteps = settings.PositionSolverIterations;
		joltSettings.mNumVelocitySteps = settings.VelocitySolverIterations;
		m_JoltSystem->SetPhysicsSettings(joltSettings);

		m_JoltContactListener = CreateScope<JoltContactListener>(&m_JoltSystem->GetBodyLockInterfaceNoLock(), [this](ContactType contactType, UUID entityA, UUID entityB)
		{
			// TODO: OnContactEvent(); This is related to scripting so won't be 100% implemented for now
		});

		m_JoltSystem->SetContactListener(m_JoltContactListener.get());
		
		CreateRigidBodies();
	}

	PhysicsScene::~PhysicsScene()
	{
		Destroy();
	}

	void PhysicsScene::Destroy()
	{
		m_RigidBodies.clear();

		m_JoltContactListener.reset();
		m_JoltLayerInterface.reset();
		m_JoltSystem.reset();

		s_CurrentInstance = nullptr;
	}

	void PhysicsScene::Simulate(float ts)
	{
		PreSimulate(ts);

		if (m_CollisionSteps > 0)
		{
			m_JoltSystem->Update(PhysicsSystem::GetSettings().FixedTimestep, m_CollisionSteps, PhysicsSystem::GetTemporariesAllocator(), PhysicsSystem::GetJobSystemThreadPool());
		}

		{
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_JoltSystem->GetBodyLockInterface();
			JPH::BodyIDVector activeBodies;
			m_JoltSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);

			JPH::BodyLockMultiWrite bodyLock(bodyLockInterface, activeBodies.data(), static_cast<int>(activeBodies.size()));
			for (int i = 0; i < activeBodies.size(); i++)
			{
				JPH::Body* body = bodyLock.GetBody(i);

				// The position of kinematic rigid bodies is synced BEFORE the physics simulation by setting its velocity such that the simulation will move it to
				// the game world position. This gives a better collision response than synching the position here
				if (body == nullptr || body->IsKinematic())
					continue;

				// Synchronize the transform
				Entity entity = m_ContextScene->TryGetEntityWithUUID(body->GetUserData());
				if (!entity)
					continue;

				Ref<PhysicsBody> rigidBody = m_RigidBodies.at(entity.GetUUID());

				TransformComponent& tc = entity.GetComponent<TransformComponent>();
				
				glm::vec3 scale = tc.Scale;
				tc.Translation = Utils::FromJoltVec3(body->GetPosition());

				if (!rigidBody->IsAllRotationLocked())
				{
					tc.SetRotation(Utils::FromJoltQuat(body->GetRotation()));
				}

				m_ContextScene->ConvertToLocalSpace(entity);
				tc.Scale = scale;
			}
		}

		PostSimulate();
	}

	Ref<PhysicsBody> PhysicsScene::CreateBody(Entity entity, PhysicsBodyAddType addType)
	{
		if (!entity.HasAny<BoxColliderComponent, SphereColliderComponent, CylinderColliderComponent, CapsuleColliderComponent, CompoundColliderComponent, MeshColliderComponent>())
		{
			// NOTE: Should log this on the console
			IR_CORE_ERROR_TAG("Physics", "Entity does not have a Collider Component");
			return nullptr;
		}

		Ref<PhysicsBody> existingBody = GetExistingPhysicsBody(entity);
		if (existingBody)
			return existingBody;

		JPH::BodyInterface& bodyInterface = m_JoltSystem->GetBodyInterface();
		Ref<PhysicsBody> rigidBody = PhysicsBody::Create(bodyInterface, entity);

		if (rigidBody->m_BodyID.IsInvalid())
		{
			IR_CORE_ERROR_TAG("Physics", "Problem creating a rigid body for entity `{}` since its rigid body ID is Invalid", entity.Name());
			return nullptr;
		}

		switch (addType)
		{
			case PhysicsBodyAddType::AddImmediate:
			{
				bodyInterface.AddBody(rigidBody->m_BodyID, JPH::EActivation::Activate);
				
				break;
			}
			case PhysicsBodyAddType::AddBulk:
			{
				// TODO:
				break;
			}
		}

		m_RigidBodies[entity.GetUUID()] = rigidBody;
		
		return rigidBody;
	}

	void PhysicsScene::DestroyBody(Entity entity)
	{
		auto iter = m_RigidBodies.find(entity.GetUUID());

		if (iter == m_RigidBodies.end())
		{
			IR_CORE_ERROR_TAG("Physics", "Can not find a rigid body for requested entity `{}`", entity.Name());
			return;
		}

		iter->second->Release();
		m_RigidBodies.erase(iter);
	}

	void PhysicsScene::SetBodyType(Entity entity, PhysicsBodyType bodyType)
	{
		Ref<PhysicsBody> rigidBody = GetExistingPhysicsBody(entity);

		if (!rigidBody)
		{
			// Could happen through C# silly things like creating a RigidBodyComponent (and suppose that fails) and then setting the BodyType manually
			IR_CORE_ERROR_TAG("Physics", "Entity does not have a RigidBodyComponent");
			return;
		}

		JPH::BodyLockWrite bodyLock(m_JoltSystem->GetBodyLockInterface(), rigidBody->m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		JPH::Body& body = bodyLock.GetBody();

		if (bodyType == PhysicsBodyType::Static && body.IsActive())
			m_JoltSystem->GetBodyInterfaceNoLock().DeactivateBody(body.GetID());

		body.SetMotionType(Utils::ToJoltMotionType(bodyType));
	}

	void PhysicsScene::SynchronizePendingBodyTransforms()
	{
		// Synchronize bodies that had an explicit request to be synchronized
		for (WeakRef<PhysicsBody> body : m_BodiesScheduledForSync)
		{
			if (!body.IsValid())
				continue;

			SynchronizeBodyTransform(body);
		}

		m_BodiesScheduledForSync.clear();
	}

	void PhysicsScene::SynchronizeBodyTransform(WeakRef<PhysicsBody> body)
	{
		// NOTE: Kind of inefficient since we would be locking for every call

		if (!body.IsValid() || !body->GetEntity())
			return;

		JPH::BodyLockRead bodyLock(m_JoltSystem->GetBodyLockInterface(), body->m_BodyID);
		IR_VERIFY(bodyLock.Succeeded());

		const JPH::Body& joltBody = bodyLock.GetBody();

		Entity entity = m_ContextScene->GetEntityWithUUID(joltBody.GetUserData());
		TransformComponent& transformComp = entity.GetComponent<TransformComponent>();

		glm::vec3 scale = transformComp.Scale;
		transformComp.Translation = Utils::FromJoltVec3(joltBody.GetPosition());
		transformComp.SetRotation(Utils::FromJoltQuat(joltBody.GetRotation()));

		m_ContextScene->ConvertToLocalSpace(entity);
		transformComp.Scale = scale;
	}

	JPH::BodyInterface& PhysicsScene::GetBodyInterface(bool shouldLock)
	{
		IR_VERIFY(s_CurrentInstance);

		if (shouldLock)
			return s_CurrentInstance->m_JoltSystem->GetBodyInterface();
		else
			return s_CurrentInstance->m_JoltSystem->GetBodyInterfaceNoLock();
	}

	const JPH::BodyLockInterface& PhysicsScene::GetBodyLockInterface(bool shouldLock)
	{
		IR_VERIFY(s_CurrentInstance);

		if (shouldLock)
			return s_CurrentInstance->m_JoltSystem->GetBodyLockInterface();
		else
			return s_CurrentInstance->m_JoltSystem->GetBodyLockInterfaceNoLock();
	}

	JPH::PhysicsSystem& PhysicsScene::GetJoltSystem()
	{
		return *s_CurrentInstance->m_JoltSystem.get();
	}

	void PhysicsScene::SubStepStrategy(float ts)
	{
		const float fixedTimeStep = PhysicsSystem::GetSettings().FixedTimestep;

		if (m_Accumulator > fixedTimeStep)
			m_Accumulator = 0.0f;

		m_Accumulator += ts;
		if (m_Accumulator < fixedTimeStep)
		{
			m_CollisionSteps = 0;
			return;
		}

		m_CollisionSteps = static_cast<uint32_t>(m_Accumulator / fixedTimeStep);
		m_Accumulator -= static_cast<float>(m_CollisionSteps * fixedTimeStep);
	}

	void PhysicsScene::PreSimulate(float ts)
	{
		SubStepStrategy(ts);

		// TODO: C# OnPhysicsUpdate

		// Synchronize transforms for (static) bodies that have been moved by gameplay
		SynchronizePendingBodyTransforms();

		for (auto& [entityID, rigidBody] : m_RigidBodies)
		{
			// We simulate kinematic rigid bodies before the whole physics simulation for better collision response
			if (rigidBody->IsKinematic())
			{
				Entity entity = m_ContextScene->GetEntityWithUUID(entityID);

				TransformComponent transformComp = m_ContextScene->GetWorldSpaceTransform(entity);

				// Moving physics bodies is an expensive operation. So only do it if it is worth it
				glm::vec3 currentBodyTranslation = rigidBody->GetTranslation();
				glm::quat currentBodyRotation = rigidBody->GetRotation();
				if (
					glm::any(glm::epsilonNotEqual(currentBodyTranslation, transformComp.Translation, 0.00001f)) 
					|| 
					glm::any(glm::epsilonNotEqual(currentBodyRotation, transformComp.GetRotation(), 0.00001f)))
				{
					// Wake up sleeping kinematic bodies since Jolt does not do that automatically
					if (rigidBody->IsSleeping())
						rigidBody->SetSleepState(false);

					glm::vec3 targetTranslation = transformComp.Translation;
					glm::quat targetRotation = transformComp.GetRotation();
					if (glm::dot(currentBodyRotation, targetRotation) < 0.0f)
						targetRotation = -targetRotation;

					if (glm::any(glm::epsilonNotEqual(currentBodyRotation, targetRotation, 0.000001f)))
					{
						glm::vec3 currentBodyEuler = glm::eulerAngles(currentBodyRotation);
						glm::vec3 targetBodyEuler = glm::eulerAngles(transformComp.GetRotation());

						glm::quat rotation = transformComp.GetRotation() * glm::conjugate(currentBodyRotation);
						glm::vec3 rotationEuler = glm::eulerAngles(rotation);
					}

					rigidBody->MoveKinematic(transformComp.Translation, targetRotation, ts);
				}
			}
		}
	}

	void PhysicsScene::PostSimulate()
	{
		// TODO: C# Stuff
	}

	void PhysicsScene::CreateRigidBodies()
	{
		std::vector<UUID> compoundedEntities;

		// NOTE: Create CompoundColliderComponent for entities that have a MeshColliderComponent with a PhysicsCollisionComplexity of Default
		// and that is because we have two mesh shapes generated, one that is convex and the other that is triangle, the convex shape is used for simulations
		// and the other traingle one is used for scene queries such as RayCasts
		{
			auto view = m_ContextScene->GetAllEntitiesWith<MeshColliderComponent>();
			for (auto id : view)
			{
				Entity entity = { id, m_ContextScene.Raw() };
		
				if (entity.HasComponent<CompoundColliderComponent>())
					continue;
		
				const MeshColliderComponent& meshColliderComp = view.get<MeshColliderComponent>(id);
				if (meshColliderComp.CollisionComplexity == PhysicsCollisionComplexity::Default)
				{
					CompoundColliderComponent& compoundColliderComp = entity.AddComponent<CompoundColliderComponent>();
					compoundColliderComp.IncludeStaticChildColliders = false; // Do not include static child colliders into this compound collider
					compoundColliderComp.IsImmutable = true;
					compoundColliderComp.CompoundedColliderEntities.push_back(entity.GetUUID());
				}
			}
		}
		
		// NOTE: Get the UUID of all the colliders that belong to a compound collider
		{
			auto view = m_ContextScene->GetAllEntitiesWith<CompoundColliderComponent>();
			for (auto id : view)
			{
				Entity entity = { id, m_ContextScene.Raw() };
		
				const CompoundColliderComponent& compoundColliderComp = entity.GetComponent<CompoundColliderComponent>();
				for (UUID uuid : compoundColliderComp.CompoundedColliderEntities)
					compoundedEntities.push_back(uuid);
			}
		}

		// Now we add RigidBodyComponent to all the entities that have an explicit collider component but no RigidBodyComponent
		{
			auto view = m_ContextScene->GetAllEntitiesWith<IDComponent>();
			for (auto id : view)
			{
				Entity entity = { id, m_ContextScene.Raw() };

				if (entity.HasComponent<RigidBodyComponent>())
					continue;

				if (!entity.HasAny<BoxColliderComponent, SphereColliderComponent, CylinderColliderComponent, CapsuleColliderComponent, MeshColliderComponent>())
					continue;

				// NOTE: Here we check if the entity we are in this iteration is referenced in the compounded entites, if yes we do not want to mess with it
				// since that entity will not have its own JoltBody, instead it will be a child of another JoltBody that is the entity that has the CompoundColliderComponent
				auto found = std::find_if(compoundedEntities.begin(), compoundedEntities.end(), [entity](const UUID& uuid)
				{
					return entity.GetUUID() == uuid;
				});

				if (found != compoundedEntities.end() && !entity.HasComponent<CompoundColliderComponent>())
					continue;

				RigidBodyComponent& rigidBodyComp = entity.AddComponent<RigidBodyComponent>();
				rigidBodyComp.BodyType = PhysicsBodyType::Static;
			}
		}

		// Finally we can create the rigid bodies for entities with a RigidBodyComponent which at this point should be all entities that have a collider component
		// only as well
		{
			auto view = m_ContextScene->GetAllEntitiesWith<RigidBodyComponent>();
			for (auto id : view)
			{
				Entity entity = { id, m_ContextScene.Raw() };

				// NOTE: Here we check if the entity we are in this iteration is referenced in the compounded entites, if yes we do not want to mess with it
				// since that entity will not have its own JoltBody, instead it will be a child of another JoltBody that is the entity that has the CompoundColliderComponent
				auto found = std::find_if(compoundedEntities.begin(), compoundedEntities.end(), [entity](const UUID& uuid)
				{
					return entity.GetUUID() == uuid;
				});

				if (found != compoundedEntities.end() && !entity.HasComponent<CompoundColliderComponent>())
					continue;

				CreateBody(entity, PhysicsBodyAddType::AddImmediate);
			}
		}
	}

}