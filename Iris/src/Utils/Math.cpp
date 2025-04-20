#include "IrisPCH.h"
#include "Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Iris::Math {

	bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
	{
		glm::mat4 LocalMatrix(transform);

		// Normalize the matrix.
		if (glm::epsilonEqual(LocalMatrix[3][3], 0.0f, glm::epsilon<float>()))
			return false;

		for (glm::length_t i = 0; i < 4; ++i)
			for (glm::length_t j = 0; j < 4; ++j)
				LocalMatrix[i][j] /= LocalMatrix[3][3];

		// Assume matrix is already normalized
		IR_ASSERT(glm::epsilonEqual(LocalMatrix[3][3], 1.0f, 0.00001f));

		// Ignore perspective
		IR_ASSERT(
			glm::epsilonEqual(LocalMatrix[0][3], 0.0f, glm::epsilon<float>()) &&
			glm::epsilonEqual(LocalMatrix[1][3], 0.0f, glm::epsilon<float>()) &&
			glm::epsilonEqual(LocalMatrix[2][3], 0.0f, glm::epsilon<float>())
		);

		// Next take care of translation (easy).
		translation = glm::vec3(LocalMatrix[3]);
		LocalMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, LocalMatrix[3].w);

		glm::vec3 Row[3];

		// Now get scale and shear.
		for (glm::length_t i = 0; i < 3; ++i)
			for (glm::length_t j = 0; j < 3; ++j)
				Row[i][j] = LocalMatrix[i][j];

		// Compute X scale factor and normalize first row.
		scale.x = glm::length(Row[0]);// v3Length(Row[0]);
		Row[0] = glm::detail::scale(Row[0], 1.0f);

		// Now, compute Y scale and normalize 2nd row.
		scale.y = glm::length(Row[1]);
		Row[1] = glm::detail::scale(Row[1], 1.0f);

		// Next, get Z scale and normalize 3rd row.
		scale.z = glm::length(Row[2]);
		Row[2] = glm::detail::scale(Row[2], 1.0f);

		// At this point, the matrix (in rows[]) is orthonormal.
		// Check for a coordinate system flip.  If the determinant
		// is -1, then negate the matrix and the scaling factors.
		glm::vec3 Pdum3 = glm::cross(Row[1], Row[2]); // v3Cross(row[1], row[2], Pdum3);
		if (glm::dot(Row[0], Pdum3) < 0)
		{
			for (glm::length_t i = 0; i < 3; i++)
			{
				scale[i] *= -1.0f;
				Row[i] *= -1.0f;
			}
		}

		int i, j, k = 0;
		float root, trace = Row[0].x + Row[1].y + Row[2].z;
		if (trace > 0.0f)
		{
			root = glm::sqrt(trace + 1.0f);
			rotation.w = 0.5f * root;
			root = 0.5f / root;
			rotation.x = root * (Row[1].z - Row[2].y);
			rotation.y = root * (Row[2].x - Row[0].z);
			rotation.z = root * (Row[0].y - Row[1].x);
		}
		else
		{
			static int Next[3] = { 1, 2, 0 };
			i = 0;
			if (Row[1].y > Row[0].x) i = 1;
			if (Row[2].z > Row[i][i]) i = 2;
			j = Next[i];
			k = Next[j];

			// Since the quat format is WXYZ
			int off = 1;

			root = glm::sqrt(Row[i][i] - Row[j][j] - Row[k][k] + 1.0f);

			rotation[i + off] = 0.5f * root;
			root = 0.5f / root;
			rotation[j + off] = root * (Row[i][j] + Row[j][i]);
			rotation[k + off] = root * (Row[i][k] + Row[k][i]);
			rotation.w = root * (Row[j][k] - Row[k][j]);
		}

		return true;
	}

	glm::mat4 ComposeTransform(const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
	{
		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}

	bool DecomposeTransformSimple(const glm::mat4& transform, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
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

		scale[0] = glm::length(glm::vec3(transform[0]));
		scale[1] = glm::length(glm::vec3(transform[1]));
		scale[2] = glm::length(glm::vec3(transform[2]));

		const glm::mat3 rotMatrix(glm::vec3(transform[0]) / scale[0], glm::vec3(transform[1]) / scale[1], glm::vec3(transform[2]) / scale[2]);
		rotation = glm::quat_cast(rotMatrix);

		return true;
	}

}