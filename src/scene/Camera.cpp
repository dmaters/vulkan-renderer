

#include "Camera.hpp"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/string_cast.hpp>

void Camera::rotate(glm::vec2 rotate) {
	m_yaw += rotate.x * 10;
	m_pitch += rotate.y * 10;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
}
glm::mat3 Camera::getOrientation() const {
	glm::quat pitchQuat =
		glm::angleAxis(glm::radians(m_pitch), glm::vec3(1, 0, 0));
	glm::quat yawQuat = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0, 1, 0));
	glm::mat3 orientation = glm::mat3_cast(yawQuat * pitchQuat);
	return orientation;
}

glm::mat4 Camera::getViewMatrix() const {
	glm::mat4 view = glm::lookAtRH(
		getOrientation() * m_position, glm::vec3(0), glm::vec3(0, 1, 0)
	);

	return view;
};

glm::mat4 Camera::getProjectionMatrix() const {
	glm::mat4 proj = glm::perspectiveRH_ZO(
		glm::radians(m_vfov),
		(float)m_resolution.x / m_resolution.y,
		0.1f,
		m_frustumPlanes[3]
	);
	return proj;
}

Camera::Fov Camera::getFov() const {
	return {
		.horizontal = m_resolution.x * m_vfov / m_resolution.y,
		.vertical = m_vfov,
	};
}
