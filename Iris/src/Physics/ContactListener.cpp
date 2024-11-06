#include "IrisPCH.h"
#include "ContactListener.h"

namespace Iris {

    void JoltContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        // TODO: With scripting engine
    }

    void JoltContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        OverrideFrictionAndRestitution(inBody1, inBody2, inManifold, ioSettings);
    }

    void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
    {
        // TODO: With scripting engine
    }

    void JoltContactListener::OnCollisionBegin(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        m_ContactEventCallback(ContactType::CollisionBegin, inBody1.GetUserData(), inBody2.GetUserData());
    }

    void JoltContactListener::OnCollisionEnd(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2)
    {
        m_ContactEventCallback(ContactType::CollisionEnd, inBody1.GetUserData(), inBody2.GetUserData());
    }

    void JoltContactListener::OnTriggerBegin(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        m_ContactEventCallback(ContactType::TriggerBegin, inBody1.GetUserData(), inBody2.GetUserData());
    }

    void JoltContactListener::OnTriggerEnd(Ref<Scene> scene, const JPH::Body& inBody1, const JPH::Body& inBody2)
    {
        m_ContactEventCallback(ContactType::TriggerEnd, inBody1.GetUserData(), inBody2.GetUserData());
    }

    void JoltContactListener::OverrideFrictionAndRestitution(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        float friction1, restitution1;
        GetFrictionAndRestitution(inBody1, inManifold.mSubShapeID1, friction1, restitution1);
        
        float  friction2, restitution2;
        GetFrictionAndRestitution(inBody2, inManifold.mSubShapeID2, friction2, restitution2);

        ioSettings.mCombinedFriction = friction1 * friction2;
        ioSettings.mCombinedRestitution = glm::max(restitution1, restitution2);
    }

    void JoltContactListener::GetFrictionAndRestitution(const JPH::Body& inBody, const JPH::SubShapeID& inSubShapeId, float& outFriction, float& outRestitution) const
    {
        const JPH::PhysicsMaterial* material = inBody.GetShape()->GetMaterial(inSubShapeId);

        if (material == JPH::PhysicsMaterial::sDefault)
        {
            outFriction = inBody.GetFriction();
            outRestitution = inBody.GetRestitution();
        }
        else
        {
            // Iris Physics Material
            const JoltMaterial* customMaterial = static_cast<const JoltMaterial*>(material);
            outFriction = customMaterial->GetFriction();
            outRestitution = customMaterial->GetRestitution();
        }
    }

}