#include "Light.hpp"

#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>

#include "scene/Camera.hpp"

static const uint16_t SHADOWMAP_RES = 2048;

using FrustumPoints = std::array<std::array<glm::vec4, 4>, 4>;
FrustumPoints getFrustumPoints(const Camera& camera) {
	Camera::Fov fov = camera.getFov();

	float tanV = tan(glm::radians(fov.vertical / 2.0));
	float tanH = tan(glm::radians(fov.horizontal / 2.0));

	glm::vec3 frustumVectors[4];

	frustumVectors[0] = glm::vec3(tanH, tanV, -1.0f);
	frustumVectors[1] = glm::vec3(-tanH, tanV, -1.0f);
	frustumVectors[2] = glm::vec3(tanH, -tanV, -1.0f);
	frustumVectors[3] = glm::vec3(-tanH, -tanV, -1.0f);

	std::array<std::array<glm::vec4, 4>, 4> points;
	for (int pl = 0; pl < 4; pl++) {
		for (int pi = 0; pi < 4; pi++) {
			float distance = camera.getCascadeDistances()[pl];
			points[pl][pi] = glm::vec4(frustumVectors[pi] * distance, 1.0);
		}
	}

	return points;
}
glm::mat4 getViewMatrix(glm::mat3 orientation, glm::vec3 position) {
	orientation[2] = -orientation[2];
	glm::mat4 view = glm::transpose(orientation);
	view[3] = glm::vec4(-glm::mat3(view) * position, 1);

	return view;
};

Light::FrustumBounds Light::getFrustumBounds(const Camera& camera, float sceneSize, uint8_t cascade) const {
	FrustumPoints fp = getFrustumPoints(camera);
	std::array<glm::vec4, 8> lightSpaceBounds;

	glm::mat4 cameraToWorld = glm::inverse(camera.getViewMatrix());
	glm::mat4 lightView = getViewMatrix(orientation, position);

	glm::mat4 cameraToLightView = lightView * cameraToWorld;

	for (int i = 0; i < 4; i++) {
		lightSpaceBounds[i] = cameraToLightView * fp[0][i];
		lightSpaceBounds[4 + i] = cameraToLightView * fp[cascade + 1][i];
	}
	float right = -1e9, left = 1e9, top = -1e9, bottom = 1e9, far = -1e9, near = 1e9;

	for (int i = 0; i < 8; i++) {
		right = fmax(right, lightSpaceBounds[i].x);
		left = fmin(left, lightSpaceBounds[i].x);
		top = fmax(top, lightSpaceBounds[i].y);
		bottom = fmin(bottom, lightSpaceBounds[i].y);
		far = fmax(far, -lightSpaceBounds[i].z);
		near = fmin(near, -lightSpaceBounds[i].z);
	}

	glm::vec4 worldCenterLightSpace = lightView * glm::vec4(0, 0, 0, 1);

	right = fmin(right, worldCenterLightSpace.x + sceneSize);
	left = fmax(left, worldCenterLightSpace.x - sceneSize);
	top = fmin(top, worldCenterLightSpace.y + sceneSize);
	bottom = fmax(bottom, worldCenterLightSpace.y - sceneSize);
	far = fmax(far, -worldCenterLightSpace.z + sceneSize);
	near = fmin(near, -worldCenterLightSpace.z - sceneSize);

	float sizeX = right - left;
	float sizeY = top - bottom;
	float maxHSize = fmax(sizeX, sizeY);
	float boundsHSize = pow(2.0, ceil(log2(maxHSize)));

	float maxVSize = far - near;
	float boundsVSize = pow(2.0, ceil(log2(maxVSize)));

	// Re-snap after making square
	float worldUnitsPerTexel = boundsHSize / SHADOWMAP_RES;
	glm::vec2 center = glm::vec2((left + right) * 0.5, (bottom + top) * 0.5);
	center.x = floor(center.x / worldUnitsPerTexel) * worldUnitsPerTexel;
	center.y = floor(center.y / worldUnitsPerTexel) * worldUnitsPerTexel;

	left = center.x - boundsHSize * 0.5;
	right = center.x + boundsHSize * 0.5;
	bottom = center.y - boundsHSize * 0.5;
	top = center.y + boundsHSize * 0.5;

	worldUnitsPerTexel = boundsVSize / 2048.0;
	float vCenter = floor((far + near) * 0.5 / worldUnitsPerTexel) * worldUnitsPerTexel;

	near = vCenter - boundsVSize * 0.5;
	far = vCenter + boundsVSize * 0.5;

	return {
		.near = near,
		.far = far,
		.right = right,
		.left = left,
		.top = top,
		.bottom = bottom,
		.padding = static_cast<float>((boundsHSize - maxHSize) * 0.5),
	};
}

MaterialDefinitions::Light Light::getShaderObject(const Camera& camera, float sceneSize) const {
	std::array<glm::mat4, 3> shadowProjections;
	std::array<float, 3> shadowPaddings;
	for (int i = 0; i < 3; i++) {
		Light::FrustumBounds bounds = getFrustumBounds(camera, sceneSize, i);
		// Top bottom swapped for y-down
		glm::mat4 proj = glm::orthoRH_ZO(bounds.left, bounds.right, bounds.bottom, bounds.top, bounds.near, bounds.far);
		shadowProjections[i] = proj;
		shadowPaddings[i] = bounds.padding;
	}
	glm::mat4 view = getViewMatrix(orientation, position);

	return MaterialDefinitions::Light {
		.view = view,
		.cascadeProjections = shadowProjections,
		.color = glm::vec3(color),
		.intensity = intensity,
		.cascadePaddings = shadowPaddings,
	};
}
