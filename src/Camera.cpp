

#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

void Camera::rotate(glm::vec2 rotate) {
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
	m_yaw += rotate.x;
	m_pitch += rotate.y;
}

glm::mat4 Camera::getViewVector() {
	glm::vec4 rotatedPosition =
		glm::vec4(m_position, 0) * (glm::eulerAngleXY(m_pitch, m_yaw));

	glm::mat4 viewMatrix = glm::lookAt(
		glm::vec3(rotatedPosition),
		glm::vec3(0),
		glm::vec3(0, 1, 0)  // Up vector
	);

	return viewMatrix;
};