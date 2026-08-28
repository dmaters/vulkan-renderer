#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace MaterialDefinitions {

struct EnvironmentData {
	float sceneSize;
};

struct PBRInstance {
	uint32_t albedoTexture = 0;
	uint32_t normalTexture = 1;
	uint32_t roughnessMetallicTexture = 2;
	float roughnessValue = 1;
	glm::vec3 albedoValue = { 1, 1, 1 };
	float metallicValue = 0;
};

}  // namespace MaterialDefinitions
