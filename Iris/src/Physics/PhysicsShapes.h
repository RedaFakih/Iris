#pragma once

#include "PhysicsTypes.h"
#include "PhysicsUtils.h"
#include "Scene/Entity.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

namespace Iris {

	class PhysicsShape : public RefCountedObject
	{
	public:
		virtual ~PhysicsShape() = default;

		virtual uint32_t GetNumShapes() const = 0;
		virtual void* GetNativeShape(uint32_t index = 0) const = 0;

		virtual void SetMaterial(const ColliderMaterial& material) = 0;

		virtual void Destroy() = 0;

		Entity GetEntity() const { return m_Entity; }
		PhysicsShapeType GetType() const { return m_Type; }

	protected:
		PhysicsShape(PhysicsShapeType type, Entity entity)
			: m_Type(type), m_Entity(entity) {}

	protected:
		Entity m_Entity;

	private:
		PhysicsShapeType m_Type;

	};

	class BoxPhysicsShape : public PhysicsShape
	{
	public:
		BoxPhysicsShape(Entity entity, float totalBodyMass);
		~BoxPhysicsShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<BoxPhysicsShape> Create(Entity entity, float totalBodyMass)
		{
			return CreateRef<BoxPhysicsShape>(entity, totalBodyMass);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		glm::vec3 GetHalfSize() const { return Utils::FromJoltVec3(reinterpret_cast<const JPH::BoxShape*>(m_Shape->GetInnerShape())->GetHalfExtent()); }

	private:
		JoltMaterial* m_Material = nullptr;
		JPH::Ref<JPH::BoxShapeSettings> m_Settings;
		JPH::Ref<JPH::RotatedTranslatedShape> m_Shape = nullptr;

	};

	class SpherePhysicsShape : public PhysicsShape
	{
	public:
		SpherePhysicsShape(Entity entity, float totalBodyMass);
		~SpherePhysicsShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<SpherePhysicsShape> Create(Entity entity, float totalBodyMass)
		{
			return CreateRef<SpherePhysicsShape>(entity, totalBodyMass);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		float GetRadius() const { return(reinterpret_cast<const JPH::SphereShape*>(m_Shape->GetInnerShape())->GetRadius()); }

	private:
		JoltMaterial* m_Material = nullptr;
		JPH::Ref<JPH::SphereShapeSettings> m_Settings;
		JPH::Ref<JPH::RotatedTranslatedShape> m_Shape = nullptr;

	};

	class CylinderPhysicsShape : public PhysicsShape
	{
	public:
		CylinderPhysicsShape(Entity entity, float totalBodyMass);
		~CylinderPhysicsShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<CylinderPhysicsShape> Create(Entity entity, float totalBodyMass)
		{
			return CreateRef<CylinderPhysicsShape>(entity, totalBodyMass);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		float GetRadius() const { return(reinterpret_cast<const JPH::CylinderShape*>(m_Shape->GetInnerShape())->GetRadius()); }
		float GetHalfHeight() const { return(reinterpret_cast<const JPH::CylinderShape*>(m_Shape->GetInnerShape())->GetHalfHeight()); }

	private:
		JoltMaterial* m_Material = nullptr;
		JPH::Ref<JPH::CylinderShapeSettings> m_Settings;
		JPH::Ref<JPH::RotatedTranslatedShape> m_Shape = nullptr;

	};

	class CapsulePhysicsShape : public PhysicsShape
	{
	public:
		CapsulePhysicsShape(Entity entity, float totalBodyMass);
		~CapsulePhysicsShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<CapsulePhysicsShape> Create(Entity entity, float totalBodyMass)
		{
			return CreateRef<CapsulePhysicsShape>(entity, totalBodyMass);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		float GetRadius() const { return(reinterpret_cast<const JPH::CapsuleShape*>(m_Shape->GetInnerShape())->GetRadius()); }
		float GetHalfHeight() const { return(reinterpret_cast<const JPH::CapsuleShape*>(m_Shape->GetInnerShape())->GetHalfHeightOfCylinder()); }

	private:
		JoltMaterial* m_Material = nullptr;
		JPH::Ref<JPH::CapsuleShapeSettings> m_Settings;
		JPH::Ref<JPH::RotatedTranslatedShape> m_Shape = nullptr;

	};

	class CompoundShape : public PhysicsShape
	{
	public:
		CompoundShape(Entity entity, bool isImmutable)
			: PhysicsShape(isImmutable ? PhysicsShapeType::CompoundShape : PhysicsShapeType::MutableCompoundShape, entity) {}
		virtual ~CompoundShape() = default;

		virtual void Create() = 0;
		virtual void AddShape(Ref<PhysicsShape> shape) = 0;
		virtual void RemoveShape(Ref<PhysicsShape> shape) = 0;

	};

	class MutableCompoundShape : public CompoundShape
	{
	public:
		MutableCompoundShape(Entity entity)
			: CompoundShape(entity, false) {}

		~MutableCompoundShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<MutableCompoundShape> Create(Entity entity)
		{
			return CreateRef<MutableCompoundShape>(entity);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		virtual void Create() override;
		virtual void AddShape(Ref<PhysicsShape> shape) override;
		virtual void RemoveShape(Ref<PhysicsShape> shape) override;

	private:
		JPH::MutableCompoundShapeSettings m_Settings;
		JPH::Ref<JPH::MutableCompoundShape> m_Shape;

	};

	class ImmutableCompoundShape : public CompoundShape
	{
	public:
		ImmutableCompoundShape(Entity entity)
			: CompoundShape(entity, true) {}

		~ImmutableCompoundShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<ImmutableCompoundShape> Create(Entity entity)
		{
			return CreateRef<ImmutableCompoundShape>(entity);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

		virtual void Create() override;
		virtual void AddShape(Ref<PhysicsShape> shape) override;
		virtual void RemoveShape(Ref<PhysicsShape> shape) override;

	private:
		JPH::StaticCompoundShapeSettings m_Settings;
		JPH::Ref<JPH::StaticCompoundShape> m_Shape;

	};

	class ConvexMeshShape : public PhysicsShape
	{
	public:
		ConvexMeshShape(Entity entity, float totalBodyMass);
		~ConvexMeshShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<ConvexMeshShape> Create(Entity entity, float totalBodyMass)
		{
			return CreateRef<ConvexMeshShape>(entity, totalBodyMass);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

	private:
		JPH::Ref<JPH::ScaledShape> m_Shape;

	};

	class TriangleMeshShape : public PhysicsShape
	{
	public:
		TriangleMeshShape(Entity entity);
		~TriangleMeshShape()
		{
			Destroy();
		}

		[[nodiscard]] inline static Ref<TriangleMeshShape> Create(Entity entity)
		{
			return CreateRef<TriangleMeshShape>(entity);
		}

		virtual uint32_t GetNumShapes() const override { return 1; }
		virtual void* GetNativeShape(uint32_t index = 0) const override { return m_Shape.GetPtr(); }

		virtual void SetMaterial(const ColliderMaterial& material) override;

		virtual void Destroy() override {}

	private:
		JPH::Ref<JPH::StaticCompoundShape> m_Shape;

	};

}