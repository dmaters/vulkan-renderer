#include "PrimitiveManager.hpp"

#include <cstdint>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

void PrimitiveManager::addPrimitive(
	std::vector<Vertex>& vertices,
	std::vector<uint32_t>& indices,
	uint32_t& vertexByteOffset,
	uint32_t& indexByteOffset
) {
	vertexByteOffset = m_positions.size();

	for (auto& vertex : vertices) {
		m_positions.push_back(vertex.position);
		m_attributes.push_back(vertex.attributes);
	}
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
				.size =
					(uint32_t)(m_attributes.size() * sizeof(Vertex::position)),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
			{
				.size = (uint32_t)(m_attributes.size() *
	                               (sizeof(Vertex::Attributes))),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
			{
				.size = (uint32_t)(m_indexBuffer.size() * sizeof(uint32_t)),
				.usage = vk::BufferUsageFlagBits::eIndexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
			{
				.size = (uint32_t)(m_instances.size() * sizeof(glm::mat4)),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                     vk::BufferUsageFlagBits::eTransferDst,
			},
		}
	);

	auto buffers = resourceManager.getBuffers(index);

	resourceManager.setName("vertex_buffer_positions", buffers.at(0));
	resourceManager.setName("vertex_buffer_attributes", buffers.at(1));
	resourceManager.setName("index_buffer", buffers.at(2));
	resourceManager.setName("instance_buffer", buffers.at(3));

	resourceManager.updateBufferSync(buffers.at(0), [&](std::byte* address) {
		memcpy(
			address,
			m_positions.data(),
			m_positions.size() * sizeof(Vertex::position)
		);
	});
	resourceManager.updateBufferSync(buffers.at(1), [&](std::byte* address) {
		memcpy(
			address,
			m_attributes.data(),
			m_attributes.size() * sizeof(Vertex::Attributes)
		);
	});
	resourceManager.updateBufferSync(buffers.at(2), [&](std::byte* address) {
		memcpy(
			address,
			m_indexBuffer.data(),
			m_indexBuffer.size() * sizeof(uint32_t)
		);
	});
	resourceManager.updateBufferSync(buffers.at(3), [&](std::byte* address) {
		memcpy(
			address, m_instances.data(), m_instances.size() * sizeof(glm::mat4)
		);
	});

	return index;
}