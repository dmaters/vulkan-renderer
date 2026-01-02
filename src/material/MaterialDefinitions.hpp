#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace MaterialDefinitions {

struct EnvironmentData {
	float sceneSize;
};

struct Camera {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 invView;
	glm::mat4 invProj;
	std::array<std::array<glm::vec4, 4>, 4> frustumPoints;
};

struct Light {
	glm::mat4 view;
	glm::vec3 color;
	float intensity;
};

struct Lights {
	Light light;
};

struct LightingMaterial {};

struct PBRInstance {
	uint32_t albedoTexture = 0;
	uint32_t normalTexture = 1;
	uint32_t roughnessMetallicTexture = 2;
	glm::vec3 albedoValue = { 1, 1, 1 };
	float roughnessValue = 1;
	float metallicValue = 0;
};

struct SimpleMaterial {};
struct ErrorFallback {};

struct GBufferBase {};

struct DeferredLighting {};

struct ShadowMap {};

struct CompositionPass {};

struct TransmittanceLUT {};
struct SkyViewLUT {};
struct MultiscatteringLUT {};

struct Skybox {};
struct SkyLighting {};

}  // namespace MaterialDefinitions
