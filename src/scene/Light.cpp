#include "Light.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

MaterialDefinitions::Light Light::getShaderObject() const {
	glm::mat4 transformation = glm::mat4(1);
	glm::mat4 orientationMat = orientation;
	glm::mat4 translationMat = glm::translate(transformation, position);

	return MaterialDefinitions::Light {
		.tranformation = translationMat * orientationMat,
		.color = glm::vec3(color),
		.intensity = intensity,
	};
}