#include "Camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

std::array<glm::float3, 4> computeFrustumDirections(Camera::Fov fov) {
	float tanV = tan(glm::radians(fov.vertical / 2.0));
	float tanH = tan(glm::radians(fov.horizontal / 2.0));

	std::array<glm::float3, 4> directions;
	directions[0] = glm::normalize(glm::vec3(tanH, tanV, -1.0f));
	directions[1] = glm::normalize(glm::vec3(-tanH, tanV, -1.0f));
	directions[2] = glm::normalize(glm::vec3(-tanH, -tanV, -1.0f));
	directions[3] = glm::normalize(glm::vec3(tanH, -tanV, -1.0f));
	return directions;
}

std::array<glm::float3, 4> computeFrustumPlanes(
	std::array<glm::float3, 4>& frustumDirections
) {
	std::array<glm::float3, 4> planesDirection;
	for (int i = 0; i < 4; i++) {
		planesDirection[i] = glm::normalize(
			glm::cross(frustumDirections[i], frustumDirections[(i + 1) % 4])
		);
	}

	return planesDirection;
}

void Camera::updateFrustum() {
	m_frustumEdgeDirections = computeFrustumDirections(m_fov);
	m_baseFrustumPlanes = computeFrustumPlanes(m_frustumEdgeDirections);
}

void Camera::rotate(glm::vec2 rotate) {
	m_yaw += rotate.x * 10;
	m_pitch += rotate.y * 10;
	m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);
}

void Camera::setResolution(glm::ivec2 resolution) {
	m_resolution = resolution;

	updateFrustum();
}

glm::mat3 Camera::getOrientation() const {
	glm::quat pitchQuat =
		glm::angleAxis(glm::radians(m_pitch), glm::vec3(1, 0, 0));
	glm::quat yawQuat = glm::angleAxis(glm::radians(m_yaw), glm::vec3(0, 1, 0));
	glm::mat3 orientation = glm::mat3_cast(yawQuat * pitchQuat);
	return orientation;
}

glm::mat4 Camera::getViewMatrix() const {
	glm::mat3 orientation = getOrientation();
	orientation = glm::transpose(orientation);
	glm::mat4 view = orientation;
	view[3] = glm::vec4(-orientation * m_position, 1);

	return view;
};

glm::mat4 Camera::getProjectionMatrix() const {
	glm::mat4 proj = glm::perspectiveRH_ZO(
		glm::radians(getFov().vertical),
		(float)m_resolution.x / m_resolution.y,
		0.1f,
		m_frustumPlanesDistances[3]
	);
	proj[1][1] *= -1;
	return proj;
}
std::array<glm::vec4, 6> Camera::getFrustumPlanes() const {
	std::array<glm::vec4, 6> planes;
	glm::mat3 orientation = getOrientation();
	for (int i = 0; i < 4; i++) {
		planes[i] = glm::vec4(orientation * m_baseFrustumPlanes[i], 0);
	}
	glm::vec3 direction = glm::vec3(0, 0, -1);

	planes[4] =
		glm::vec4(orientation * -direction, m_frustumPlanesDistances[0]);
	planes[5] = glm::vec4(orientation * direction, m_frustumPlanesDistances[3]);
	return planes;
}

using FrustumPoints = std::array<std::array<glm::vec4, 4>, 4>;
FrustumPoints Camera::getFrustumPoints(const Camera& camera) {
	std::array<std::array<glm::vec4, 4>, 4> points;
	glm::mat4 invView = glm::inverse(camera.getViewMatrix());
	for (int pl = 0; pl < 4; pl++) {
		for (int pi = 0; pi < 4; pi++) {
			float distance = camera.getCascadeDistances()[pl];
			points[pl][pi] =
				invView *
				glm::vec4(m_frustumEdgeDirections[pi] * distance, 1.0);
		}
	}

	return points;
}
