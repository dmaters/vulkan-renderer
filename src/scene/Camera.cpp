

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
#include <iostream>
#include <numbers>

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
		glm::radians(90.f), (float)m_resolution.x / m_resolution.y, 0.1f, 720.0f
	);
	return proj;
}

std::array<glm::vec4, 6> Camera::getFrustum() const {
	glm::mat4 viewMatrix = glm::mat4(1);

	glm::vec3 right = viewMatrix[0];
	glm::vec3 up = viewMatrix[1];
	glm::vec3 direction = -viewMatrix[2];

	std::array<glm::vec4, 6> planes;
	float aspectRatio = ((float)m_resolution.x / m_resolution.y);

	float vertical = std::sin(glm::radians(m_fov * 0.5));
	float horizontal = vertical * aspectRatio;

	// Near and Far planes
	planes[0] = glm::vec4(-direction, 0.1);
	planes[1] = glm::vec4(direction, 600);

	// Left and Right planes
	glm::vec3 leftNormal =
		glm::normalize(-glm::cross(up, direction + right * horizontal));
	planes[2] = glm::vec4(leftNormal, 0);

	glm::vec3 rightNormal =
		glm::normalize(-glm::cross(up, direction - right * horizontal));
	planes[3] = glm::vec4(rightNormal, 0);

	// Top and Bottom planes

	// ORDer inverted
	glm::vec3 topNormal =
		glm::normalize(glm::cross(right, direction - up * vertical));
	planes[4] = glm::vec4(topNormal, 0);

	glm::vec3 bottomNormal =
		glm::normalize(-glm::cross(right, direction + up * vertical));
	planes[5] = glm::vec4(bottomNormal, 0);

	return planes;
}

std::array<glm::vec4, 8> Camera::getFrustumBounds() const {
	std::array<glm::vec4, 6> planes = getFrustum();

	std::array<glm::vec4, 8> bounds;

	for (int i = 0; i < 8; i++) {
		glm::uvec3 pc =
			glm::uvec3((i >> 0u) & 1u, (i >> 1u) & 1u, (i >> 2u) & 1u);

		glm::mat3 base = glm::transpose(
			glm::mat3(planes[pc.x], planes[pc.y + 2], planes[pc.z + 4])
		);

		float baseDet = glm::determinant(base);
		glm::vec3 coefficents =
			glm::vec3(planes[pc.x].w, planes[pc.y + 2].w, planes[pc.z + 4].w);

		glm::vec3 position;

		glm::mat3 x = base;
		x[0] = coefficents;
		position.x = glm::determinant(x) / baseDet;

		glm::mat3 y = base;
		y[1] = coefficents;
		position.y = glm::determinant(y) / baseDet;

		glm::mat3 z = base;
		z[2] = coefficents;
		position.z = glm::determinant(z) / baseDet;

		bounds[i] = glm::vec4(getOrientation() * (position + m_position), 1);
	}

	return bounds;
}