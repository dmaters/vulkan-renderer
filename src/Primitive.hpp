#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <memory>

#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"

struct Primitive {
	Buffer vertexBuffer;
	Buffer indexBuffer;
	MaterialManager::MaterialInstance material;
	size_t indexCount;

	glm::mat4x4 modelMatrix;
};
