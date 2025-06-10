#pragma once
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

struct Material {};

struct PBRMaterial : public Material {
	struct BaseValues {
		glm::vec3 baseColor;
	};
	BaseValues* base_values;

	const Lights* light_data;
	const ViewProjection* view_projection;
};

struct SimpleMaterial {};

}  // namespace MaterialDefinitions