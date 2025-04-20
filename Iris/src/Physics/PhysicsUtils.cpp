#include "IrisPCH.h"
#include "PhysicsUtils.h"

namespace Iris::Utils {

	JPH::Vec3 ToJoltVec3(const glm::vec3& vec)
	{
		return JPH::Vec3{ vec.x, vec.y, vec.z };
	}

	JPH::Quat ToJoltQuat(const glm::quat& quat)
	{
		return JPH::Quat{ quat.x, quat.y, quat.z, quat.w };
	}

	glm::vec3 FromJoltVec3(const JPH::Vec3& vec)
	{
		return { vec.GetX(), vec.GetY(), vec.GetZ() };
	}

	glm::quat FromJoltQuat(const JPH::Quat& quat)
	{
		return glm::quat(quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ());
	}

	JPH::EMotionType ToJoltMotionType(PhysicsBodyType type)
	{
		switch (type)
		{
			case PhysicsBodyType::Static:		return JPH::EMotionType::Static;
			case PhysicsBodyType::Dynamic:		return JPH::EMotionType::Dynamic;
			case PhysicsBodyType::Kinematic:	return JPH::EMotionType::Kinematic;
		}

		return JPH::EMotionType::Static;
	}

	JPH::EMotionQuality ToJoltMotionQuality(PhysicsCollisionDetectionType type)
	{
		switch (type)
		{
			case PhysicsCollisionDetectionType::Discrete:	return JPH::EMotionQuality::Discrete;
			case PhysicsCollisionDetectionType::Continuous: return JPH::EMotionQuality::LinearCast;
		}

		return JPH::EMotionQuality::Discrete;
	}

}