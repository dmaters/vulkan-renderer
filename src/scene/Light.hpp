#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "material/MaterialDefinitions.hpp"

struct Light {
	enum class Type {
		Directional,
		Point
	};

	glm::vec3 position = glm::vec3(0);
	glm::mat3 orientation = glm::mat3(orientation);

	glm::vec3 color = glm::vec3(1);
	float intensity = 1;
	Type type = Type::Directional;
	bool shadows;

	MaterialDefinitions::Light getShaderObject() const;
};
