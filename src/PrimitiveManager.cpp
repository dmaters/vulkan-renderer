#include "PrimitiveManager.hpp"

#include <cstdint>
#include <vulkan/vulkan_enums.hpp>

#include "memory/MemoryAllocator.hpp"

void PrimitiveManager::addPrimitive(
	std::vector<std::byte> vertices,
	std::vector<std::byte> indices,
	uint32_t& vertexOffset,
	uint32_t& indexOffset
) {
	vertexOffset = m_vertexbuffer.size();
	m_vertexbuffer.insert(
		m_vertexbuffer.end(), vertices.begin(), vertices.end()
	);
	indexOffset = m_indexBuffer.size() / sizeof(uint16_t);
	m_indexBuffer.insert(m_indexBuffer.end(), indices.begin(), indices.end());
}
bool PrimitiveManager::buildBuffers(
	ResourceManager& resourceManager, Buffer& vertexBuffer, Buffer& indexBuffer
) {
	vertexBuffer = resourceManager.createBuffer({
		.size = m_vertexbuffer.size(),
		.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	             vk::BufferUsageFlagBits::eTransferDst,
		.location = MemoryAllocator::Location::DeviceLocal,
	});

	resourceManager.copyToBuffer(m_vertexbuffer, vertexBuffer);

	indexBuffer = resourceManager.createBuffer({
		.size = m_indexBuffer.size(),
		.usage = vk::BufferUsageFlagBits::eIndexBuffer |
	             vk::BufferUsageFlagBits::eTransferDst,
		.location = MemoryAllocator::Location::DeviceLocal,
	});
	resourceManager.copyToBuffer(m_indexBuffer, indexBuffer);
	return true;
}