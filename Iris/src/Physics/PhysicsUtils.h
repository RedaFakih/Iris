#pragma once

#include "PhysicsTypes.h"

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/MotionQuality.h>

#include <glm/glm.hpp>

namespace Iris::Utils {

	JPH::Vec3 ToJoltVec3(const glm::vec3& vec);
	JPH::Quat ToJoltQuat(const glm::quat& quat);

	glm::vec3 FromJoltVec3(const JPH::Vec3& vec);
	glm::quat FromJoltQuat(const JPH::Quat& quat);

	JPH::EMotionType ToJoltMotionType(PhysicsBodyType type);
	JPH::EMotionQuality ToJoltMotionQuality(PhysicsCollisionDetectionType type);

	template<typename T, typename U>
	JPH::Ref<T> CastJoltRef(JPH::RefConst<U> ref)
	{
		return JPH::Ref<T>(static_cast<T*>(const_cast<U*>(ref.GetPtr())));
	}

	template<typename T, typename U>
	JPH::Ref<T> CastJoltRef(const JPH::Ref<U> ref)
	{
		return JPH::Ref<T>(static_cast<T*>(const_cast<U*>(ref.GetPtr())));
	}

}