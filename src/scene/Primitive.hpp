#pragma once

#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

struct Vertex {
	glm::vec3 vertex;

	struct Attributes {
		glm::vec3 normal;
		uint32_t _padding0;
		glm::vec3 tangent;
		uint32_t _padding1;
		glm::vec3 bitangent;
		uint32_t _padding2;
		glm::vec2 texcoord;
		uint32_t _padding3[2];
	};
};

using PrimitiveIndex = std::uint32_t;
struct Primitive {
	uint32_t baseVertex;
	uint32_t baseIndex;

	uint32_t vertexCount;
	uint32_t indexCount;

	uint32_t materialIndex;

	uint32_t baseTransform;

	struct ShaderObject {
		uint32_t baseVertex;
		uint32_t baseIndex;
		uint32_t materialIndex;
	};
};
