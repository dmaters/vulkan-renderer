#include "Light.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>

MaterialDefinitions::Light Light::getShaderObject() const {
	glm::mat4 view =
		glm::lookAt(orientation * position, glm::vec3(0), orientation[1]);

	return MaterialDefinitions::Light {
		.view = view,
		.color = glm::vec3(color),
		.intensity = intensity,
	};
}