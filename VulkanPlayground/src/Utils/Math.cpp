#include "vkPch.h"
#include "Math.h"

namespace vkPlayground::Math {

	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
	{
		// Lets start with translation: it is equal to the elements of the last column, we get these and then 0 out that column
		glm::mat4 Local(transform);

		// Normalize the matrix
		if (glm::epsilonEqual(Local[3][3], 0.0f, glm::epsilon<float>()))
			return false;

		translation = glm::vec3(Local[3]);
		Local[3] = glm::vec4(0, 0, 0, Local[3].w);

		// Now onto rotation: Calculate the polar decomposition of Local to obtain rotation R and stretch S matrices:
		// Local = RS

		//for (int i = 0; i < 3; i++)
		//	scale[i] = glm::length(glm::vec3(transform[i]));

		scale[0] = glm::length(glm::vec3(transform[0]));
		scale[1] = glm::length(glm::vec3(transform[1]));
		scale[2] = glm::length(glm::vec3(transform[2]));

		const glm::mat3 rotMatrix(glm::vec3(transform[0]) / scale[0], glm::vec3(transform[1]) / scale[1], glm::vec3(transform[2]) / scale[2]);
		rotation = glm::quat_cast(rotMatrix);

		return true;
	}

}