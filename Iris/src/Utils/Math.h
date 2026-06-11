#pragma once

#include <glm/glm.hpp>

namespace Iris::Math {

	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale);

	template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
	inline static T DivideAndRoundUp(T dividend, T divisor)
	{
		return (dividend + divisor - 1) / divisor;
	}

	template<typename T, typename DivisorT, std::enable_if_t<std::is_same_v<T, glm::uvec2>&& std::is_integral_v<DivisorT>, bool> = true>
	inline static T DivideAndRoundUp(T dividend, DivisorT divisor)
	{
		return { DivideAndRoundUp(dividend.x, divisor), DivideAndRoundUp(dividend.y, divisor) };
	}

}