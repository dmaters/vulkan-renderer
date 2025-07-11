#include "PrimitiveManager.hpp"

#include <cstdint>
#include <vulkan/vulkan_enums.hpp>

#include "memory/MemoryAllocator.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

void PrimitiveManager::addPrimitive(
	std::vector<std::byte> vertices,
	std::vector<std::byte> indices,
	uint32_t& vertexByteOffset,
	uint32_t& indexByteOffset
) {
	vertexByteOffset = m_vertexbuffer.size();
	m_vertexbuffer.insert(
		m_vertexbuffer.end(), vertices.begin(), vertices.end()
	);
	indexByteOffset = m_indexBuffer.size();
	m_indexBuffer.insert(m_indexBuffer.end(), indices.begin(), indices.end());
}
bool PrimitiveManager::buildBuffers(ResourceManager& resourceManager) {
	BufferHandle vertexBuffer = resourceManager.createBuffer({
		.size = (uint32_t)m_vertexbuffer.size(),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	             vk::BufferUsageFlagBits::eTransferDst,
		.location = AllocationLocation::Device,
	});
	resourceManager.setName("vertex_buffer", vertexBuffer);
	resourceManager.copyToBuffer(m_vertexbuffer, vertexBuffer);

	BufferHandle indexBuffer = resourceManager.createBuffer({
		.size = (uint32_t)m_indexBuffer.size(),
		.usage = vk::BufferUsageFlagBits::eIndexBuffer |
	             vk::BufferUsageFlagBits::eTransferDst,
		.location = AllocationLocation::Device,
	});
	resourceManager.setName("index_buffer", indexBuffer);

	resourceManager.copyToBuffer(m_indexBuffer, indexBuffer);
	return true;
}