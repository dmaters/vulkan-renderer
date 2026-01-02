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
	uint32_t albedo;
	uint32_t normal;
	uint32_t roughness_metallic;
	int32_t _padding;
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
