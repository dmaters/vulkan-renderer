#pragma once

#include <glm/glm.hpp>

#include "material/MaterialDefinitions.hpp"
#include "scene/Camera.hpp"

struct Light {
	glm::vec3 position = glm::vec3(0);
	glm::mat3 orientation = glm::mat3(orientation);

	glm::vec3 color = glm::vec3(1);
	float intensity = 1.0f;

	std::array<glm::vec4, 6> frustumPlanes;

	struct FrustumBounds {
		float near;
		float far;
		float right;
		float left;
		float top;
		float bottom;
		float padding;
	};

	FrustumBounds getFrustumBounds(const Camera& camera, float sceneSize, uint8_t cascade) const;

	MaterialDefinitions::Light getShaderObject(const Camera& camera, float sceneSize) const;
};
