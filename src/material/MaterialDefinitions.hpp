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
struct Skybox {};

}  // namespace MaterialDefinitions