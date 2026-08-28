#pragma once

#include <array>
#include <glm/glm.hpp>

#include "scene/Camera.hpp"

struct Light {
	glm::vec3 position = glm::vec3(0);
	glm::mat3 orientation = glm::mat3(1);

	glm::vec3 color = glm::vec3(1);
	float intensity = 1.0f;

	struct ShaderObject {
		glm::mat4 view;
		std::array<glm::mat4, 3> cascadeProjections;
		glm::vec3 color;
		float intensity;
		std::array<float, 4> cascadeDistances;
		std::array<float, 3> cascadePaddings;
		int32_t _padding;
	};

	ShaderObject getShaderObject(const Camera& camera, const Camera::ShaderObject& cameraObject, float sceneSize) const;
};
