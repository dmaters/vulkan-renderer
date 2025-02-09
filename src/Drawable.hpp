#pragma once

#include <vk_mem_alloc.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>

#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"

struct Drawable {
	MemoryAllocator::BufferAllocation m_vertexBuffer;
	MemoryAllocator::BufferAllocation m_indexBuffer;
	MaterialHandle m_materialInstance;
	glm::mat4x4 m_modelMatrix;
	size_t m_indexCount;
};

/*
class Drawable {
private:


public:
    Drawable(
        Buffer vertexBuffer, Buffer indexBuffer, MaterialInstance instance
    );

    inline void setModelMatrix(glm::mat4x4 modelMatrix) {
        m_modelMatrix = modelMatrix;
    };
    inline glm::mat4x4 getModelMatrix() const { return m_modelMatrix; };

    void registerDraw(vk::CommandBuffer& commandBuffer) const;
};
*/