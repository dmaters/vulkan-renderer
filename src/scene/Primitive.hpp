#pragma once

#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec3 vertex;

	struct Attributes {
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec3 bitangent;
		glm::vec2 texcoord;
	};
};

using PrimitiveIndex = std::uint32_t;
struct Primitive {
	uint32_t baseVertex;
	uint32_t baseIndex;
	uint32_t indexCount;
	uint32_t materialIndex;

	struct ShaderObject {
		uint32_t baseVertex;
		uint32_t baseIndex;
		uint32_t materialIndex;
	};
};
