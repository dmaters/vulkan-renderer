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
};

struct Light {
	glm::mat4 view;
	std::array<glm::mat4, 3> cascadeProjections;
	glm::vec3 color;
	float intensity;
	std::array<float, 3> cascadePaddings;
	int32_t _padding;
};

struct Lights {
	Light light;
};

struct LightingMaterial {};

struct PBRInstance {
	uint32_t albedoTexture = 0;
	uint32_t normalTexture = 1;
	uint32_t roughnessMetallicTexture = 2;
	float roughnessValue = 1;
	glm::vec3 albedoValue = { 1, 1, 1 };
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
