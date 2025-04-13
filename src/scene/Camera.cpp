

#include "Camera.hpp"

#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <numbers>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

void Camera::rotate(glm::vec2 rotate) {
	m_yaw += rotate.x * 10;
	m_pitch += rotate.y * 10;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
}

glm::mat4 Camera::getViewVector() {
	glm::quat pitchQuat =
		glm::angleAxis(glm::radians(m_pitch), glm::vec3(1, 0, 0));
	glm::quat yawQuat = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0, 1, 0));

	glm::mat3 rotation = glm::mat3_cast(yawQuat * pitchQuat);

	glm::vec3 rotatedPosition = rotation * m_position;

	glm::mat4 viewMatrix = glm::lookAt(
		glm::vec3(rotatedPosition),
		glm::vec3(0),
		glm::vec3(0, 1, 0)  // Up vector
	);

	return viewMatrix;
};