#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace MaterialDefinitions {

struct Light {
	glm::mat4 tranformation;
	glm::vec3 color;
	float intensity;
};

struct Lights {
	uint32_t count;
	uint32_t _padding[3];
	Light lights[256];
};

struct ViewProjection {
	glm::mat4 view;
	glm::mat4 projection;
};

struct LightingMaterial {};

struct PBRUniform {
	uint32_t albedo;
	uint32_t normal;
	uint32_t roughness_metallic;
};
typedef std::array<PBRUniform, 512> PBRUniforms;

struct PBRMaterial {
	PBRUniforms* instances;
	const Lights* light_data;
	const ViewProjection* view_projection;
};

struct SimpleMaterial {};
struct ErrorFallback {};

struct GBufferBase {
	PBRUniforms* instances;
	const ViewProjection* view_projection;
};

struct DeferredLighting {
	const Lights* light_data;
};

}  // namespace MaterialDefinitions