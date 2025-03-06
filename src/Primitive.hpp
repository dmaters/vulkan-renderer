#pragma once

#include <vk_mem_alloc.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <memory>

#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"

struct Primitive {
	MemoryAllocator::BufferAllocation vertexBuffer;
	MemoryAllocator::BufferAllocation indexBuffer;
	MaterialManager::MaterialInstance material;
	size_t indexCount;

	glm::mat4x4 modelMatrix;
};
