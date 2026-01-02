#include "PrimitiveManager.hpp"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>

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
