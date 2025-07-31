#include "Light.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>

MaterialDefinitions::Light Light::getShaderObject() const {
	glm::mat4 view = orientation;
	view[2] = -view[2];
	view = glm::transpose(view);
	view[3] = glm::vec4(-position.x, -position.y, position.z, 1);

	return MaterialDefinitions::Light {
		.view = view,
		.color = glm::vec3(color),
		.intensity = intensity,
	};
}