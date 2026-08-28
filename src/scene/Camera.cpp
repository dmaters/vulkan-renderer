#include "Camera.hpp"

#include <array>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

float Camera::getFrustumSize(float sceneSize) const {
	glm::mat3 orientation = getOrientation();
	return glm::length(-orientation[2] * sceneSize - position);
}

std::array<glm::vec3, 4> Camera::getFrustumDirections() const {
	float tanV = tan(glm::radians(fov.y / 2.0));
	float tanH = tan(glm::radians(fov.x / 2.0));

	std::array<glm::vec3, 4> directions;
	directions[0] = glm::vec3(tanH, tanV, -1.0f);
	directions[1] = glm::vec3(-tanH, tanV, -1.0f);
	directions[2] = glm::vec3(-tanH, -tanV, -1.0f);
	directions[3] = glm::vec3(tanH, -tanV, -1.0f);

	auto orientation = getOrientation();
	for (int i = 0; i < 4; i++) {
		directions[i] = orientation * directions[i];
	}
	return directions;
}

std::array<glm::vec4, 6> Camera::getFrustumPlanes(float sceneSize) const {
	std::array<glm::vec3, 4> frustumDirections = getFrustumDirections();
	std::array<glm::vec4, 6> planes;
	for (int i = 0; i < 4; i++) {
		planes[i] = glm::vec4(glm::normalize(glm::cross(frustumDirections[i], frustumDirections[(i + 1) % 4])), 0);
	}

	auto orientation = getOrientation();
	planes[4] = glm::vec4(orientation[2], 0.1);
	planes[5] = glm::vec4(-orientation[2], getFrustumSize(sceneSize));
	return planes;
}

glm::mat3 Camera::getOrientation() const {
	glm::quat pitchQuat = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));
	glm::quat yawQuat = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
	glm::mat3 orientation = glm::mat3_cast(yawQuat * pitchQuat);
	return orientation;
}

glm::mat4 Camera::getViewMatrix() const {
	glm::mat3 orientation = this->getOrientation();
	orientation = glm::transpose(orientation);
	glm::mat4 view = orientation;
	view[3] = glm::vec4(-orientation * position, 1);

	return view;
};

glm::mat4 Camera::getProjectionMatrix(glm::ivec2 resolution, float sceneSize) const {
	glm::mat4 proj =
		glm::perspectiveRH_ZO(glm::radians(fov.y), (float)resolution.x / resolution.y, 0.1f, getFrustumSize(sceneSize));
	proj[1][1] *= -1;
	return proj;
}
