#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace MaterialDefinitions {

struct EnvironmentData {
	glm::vec3 environmentColor;
	float sceneSize;
};

struct Camera {
	glm::mat4 view;
	glm::mat4 projection;
	std::array<std::array<glm::vec4, 4>, 4> frustumPoints;
};

struct Light {
	glm::mat4 view;
	glm::vec3 color;
	float intensity;
};

struct Lights {
	uint32_t count;
	uint32_t directLightIndex;
	uint32_t _padding[2];
	Light lights[255];
};

struct LightingMaterial {};

struct PBRUniform {
	uint32_t albedo;
	uint32_t normal;
	uint32_t roughness_metallic;
	int32_t _padding;
};

typedef std::array<PBRUniform, 512> PBRUniforms;

struct PBRMaterial {
	PBRUniforms* instances;
	// const Lights* light_data;
	// const ViewProjection* view_projection;
};

struct SimpleMaterial {};
struct ErrorFallback {};

struct GBufferBase {
	PBRUniforms* instances;
	// const ViewProjection* view_projection;
};

struct DeferredLighting {
	//	const Lights* light_data;
};

struct ShadowMap {
	//	const Lights* light_data;
};

struct CompositionPass {};

struct TransmittanceLUT {};
struct SkyViewLUT {};

}  // namespace MaterialDefinitions