#include "PrimitiveManager.hpp"

#include <cstdint>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

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
ResourceManager::AllocationIndex PrimitiveManager::buildBuffers(
	ResourceManager& resourceManager
) {
	ResourceManager::AllocationIndex index = resourceManager.createResources(
		{
    },
		{
			{
				.size = (uint32_t)m_vertexbuffer.size(),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
			{
				.size = (uint32_t)m_indexBuffer.size(),
				.usage = vk::BufferUsageFlagBits::eIndexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
		}
	);

	auto buffers = resourceManager.getBuffers(index);

	resourceManager.setName("vertex_buffer", buffers.at(0));
	resourceManager.setName("index_buffer", buffers.at(1));

	resourceManager.updateBufferSync(
		buffers.at(0),
		[vertices = m_vertexbuffer](std::byte* address) {
			memcpy(address, vertices.data(), vertices.size());
		}
	);
	resourceManager.updateBufferSync(
		buffers.at(1),
		[indices = m_indexBuffer](std::byte* address) {
			memcpy(address, indices.data(), indices.size());
		}
	);
	return index;
}