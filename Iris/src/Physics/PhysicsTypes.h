#pragma once

#include "Core/Base.h"

namespace Iris {

	enum class PhysicsBodyType : uint8_t
	{
		Static = 0,
		Dynamic,
		Kinematic
	};

	enum class PhysicsBodyAddType : uint8_t
	{
		AddImmediate = 0,
		AddBulk
	};

	enum class PhysicsForceMode : uint8_t
	{
		Force = 0,
		Impulse,
		VelocityChange,
		Acceleration
	};

	enum class PhysicsFallOffMode : uint8_t
	{
		Constant = 0,
		Linear
	};

	enum class PhysicsCollisionDetectionType : uint8_t
	{
		Discrete = 0,
		Continuous
	};

	enum class PhysicsCollisionComplexity : uint8_t
	{
		Default = 0,		// Use Simple for collision and Complex for scene queries
		UseComplexAsSimple, // Use Complex for collision AND scene queries
		UseSimpleAsComplex  // Use simple for collision AND scene queries
	};

	enum class PhysicsShapeType : uint8_t
	{
		None = 0,
		Box,
		Sphere,
		Capsule,
		Cylinder,
		ConvexMesh,
		TriangleMesh,
		CompoundShape, // Immutable default
		MutableCompoundShape,
		_COUNT
	};

	enum class PhysicsActorAxis : uint8_t
	{
		None = 0,
		TranslationX = BIT(0),
		TranslationY = BIT(1),
		TranslationZ = BIT(2),
		Translation = TranslationX | TranslationY | TranslationZ,
		RotationX = BIT(3),
		RotationY = BIT(4),
		RotationZ = BIT(5),
		Rotation = RotationX | RotationY | RotationZ
	};

	enum class PhysicsCookingResult : uint8_t
	{
		None,
		Success,
		ZeroAreaTestFailed,
		PolygonLimitReached,
		LargeTriangle,
		InvalidMesh,
		Failure
	};

	namespace Utils {

		inline const char* ShapeTypeToString(PhysicsShapeType type)
		{
			switch (type)
			{
				case PhysicsShapeType::None:			     return "None";
				case PhysicsShapeType::Box:					 return "Box";
				case PhysicsShapeType::Sphere:				 return "Sphere";
				case PhysicsShapeType::Capsule:				 return "Capsule";
				case PhysicsShapeType::Cylinder:			 return "Cylinder";
				case PhysicsShapeType::ConvexMesh:			 return "ConvexMesh";
				case PhysicsShapeType::TriangleMesh:		 return "TriangleMesh";
				case PhysicsShapeType::CompoundShape:		 return "CompoundShape";
				case PhysicsShapeType::MutableCompoundShape: return "MutableCompoundShape";
			}

			IR_VERIFY(false);
			return "None";
		}

	}

}