#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct Light {
	enum class Type {
		Directional,
		Point
	};

	struct uLight;
	struct uLightData;

	glm::vec3 position = glm::vec3(0);
	glm::mat3 orientation = glm::mat3(orientation);

	glm::vec3 color = glm::vec3(1);
	float intensity = 1;
	Type type = Type::Directional;
	bool shadows;

	Light::uLight getShaderObject() const;
};

struct Light::uLight {
	glm::mat4 tranformation;
	glm::vec4 color_intensity;
};

struct Light::uLightData {
	uint32_t count;
	int _padding[3];
	uLight lights[256];
};