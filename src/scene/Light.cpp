#include "Light.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

Light::uLight Light::getShaderObject() const {
	glm::mat4 transformation = glm::mat4(1);
	glm::mat4 orientationMat = orientation;
	glm::mat4 translationMat = glm::translate(transformation, position);

	return Light::uLight {
		.tranformation = translationMat * orientationMat,
		.color_intensity = glm::vec4(color, intensity),
	};
}