#pragma once

#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

using PrimitiveIndex = std::uint32_t;
struct Primitive {
	uint32_t baseVertex;
	uint32_t baseIndex;
	uint32_t indexCount;
};
