#pragma once

#include "Scene/Scene.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/ContactListener.h>

namespace Iris {

	enum class ContactType : int8_t
	{
		None = -1,
		CollisionBegin,
		CollisionEnd,
		TriggerBegin,
		TriggerEnd
	};

	using ContactCallbackFn = std::function<void(ContactType, UUID, UUID)>;

	class JoltContactListener : public JPH::ContactListener
	{
	public:
		JoltContactListener(const JPH::BodyLockInterfaceNoLock* bodyLockInterface, const ContactCallbackFn& contactCallback)
			: m_BodyLockInterface(bodyLockInterface), m_ContactEventCallback(contactCallback) {}
		virtual ~JoltContactListener() = default;

		/// Called after detecting a collision between a body pair, but before calling OnContactAdded and before adding the contact constraint.
		/// If the function rejects the contact, the contact will not be added and any other contacts between this body pair will not be processed.
		/// This function will only be called once per PhysicsSystem::Update per body pair and may not be called again the next update
		/// if a contact persists and no new contact pairs between sub shapes are found.
		/// This is a rather expensive time to reject a contact point since a lot of the collision detection has happened already, make sure you
		/// filter out the majority of undesired body pairs through the ObjectLayerPairFilter that is registered on the PhysicsSystem.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Body 1 will have a motion type that is larger or equal than body 2's motion type (order from large to small: dynamic -> kinematic -> static). When motion types are equal, they are ordered by BodyID.
		/// The collision result (inCollisionResult) is reported relative to inBaseOffset.
		virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, [[maybe_unused]] JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override { return JPH::ValidateResult::AcceptAllContactsForThisBodyPair; }

		/// Called whenever a new contact point is detected.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		/// Note that only active bodies will report contacts, as soon as a body goes to sleep the contacts between that body and all other
		/// bodies will receive an OnContactRemoved callback, if this is the case then Body::IsActive() will return false during the callback.
		/// When contacts are added, the constraint solver has not run yet, so the collision impulse is unknown at that point.
		/// The velocities of inBody1 and inBody2 are the velocities before the contact has been resolved, so you can use this to
		/// estimate the collision impulse to e.g. determine the volume of the impact sound to play (see: EstimateCollisionResponse).
		virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);

		/// Called whenever a contact is detected that was also detected last update.
		/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		/// If the structure of the shape of a body changes between simulation steps (e.g. by adding/removing a child shape of a compound shape),
		/// it is possible that the same sub shape ID used to identify the removed child shape is now reused for a different child shape. The physics
		/// system cannot detect this, so may send a 'contact persisted' callback even though the contact is now on a different child shape. You can
		/// detect this by keeping the old shape (before adding/removing a part) around until the next PhysicsSystem::Update (when the OnContactPersisted
		/// callbacks are triggered) and resolving the sub shape ID against both the old and new shape to see if they still refer to the same child shape.
		virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);

		/// Called whenever a contact was detected last update but is not detected anymore.
		/// You cannot access the bodies at the time of this callback because:
		/// - All bodies are locked at the time of this callback.
		/// - Some properties of the bodies are being modified from another thread at the same time.
		/// - The body may have been removed and destroyed (you'll receive an OnContactRemoved callback in the PhysicsSystem::Update after the body has been removed).
		/// Cache what you need in the OnContactAdded and OnContactPersisted callbacks and store it in a separate structure to use during this callback.
		/// Alternatively, you could just record that the contact was removed and process it after PhysicsSimulation::Update.
		/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
		/// The sub shape ID were created in the previous simulation step too, so if the structure of a shape changes (e.g. by adding/removing a child shape of a compound shape),
		/// the sub shape ID may not be valid / may not point to the same sub shape anymore.
		/// If you want to know if this is the last contact between the two bodies, use PhysicsSystem::WereBodiesInContact.
		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair);

	private:
		void OnCollisionBegin(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);
		void OnCollisionEnd(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2);
		void OnTriggerBegin(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);
		void OnTriggerEnd(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2);

		void OverrideFrictionAndRestitution(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings);
		void GetFrictionAndRestitution(const JPH::Body& inBody, const JPH::SubShapeID& inSubShapeId, float& outFriction, float& outRestitution) const;

	private:
		const JPH::BodyLockInterfaceNoLock* m_BodyLockInterface = nullptr;
		ContactCallbackFn m_ContactEventCallback;

	};

}