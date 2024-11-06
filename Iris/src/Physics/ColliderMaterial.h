#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace Iris {

	struct ColliderMaterial
	{
		float Friction = 0.5f;
		float Restitution = 0.15f;
	};

	class JoltMaterial : public JPH::PhysicsMaterial
	{
	public:
		JoltMaterial() = default;
		JoltMaterial(float friction, float restitution)
			: m_Friction(friction), m_Restitution(restitution) {}

		inline static JoltMaterial* CreateFromColliderMaterial(const ColliderMaterial& colliderMaterial)
		{
			return new JoltMaterial(colliderMaterial.Friction, colliderMaterial.Restitution);
		}

		void SetFriction(float friction) { m_Friction = friction; }
		float GetFriction() const { return m_Friction; }

		void SetRestitution(float restitution) { m_Restitution = restitution; }
		float GetRestitution() const { return m_Restitution; }

	private:
		float m_Friction = 0.5f;
		float m_Restitution = 0.15f;
	};

}