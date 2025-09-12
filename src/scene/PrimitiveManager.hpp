#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Instance.hpp"
#include "Primitive.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

struct Vertex {
	glm::vec3 position;
	struct Attributes {
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec3 bitangent;
		glm::vec2 texcoord;
	};
	Attributes attributes;
};

class PrimitiveManager {
private:
	std::vector<glm::vec3> m_positions;
	std::vector<Vertex::Attributes> m_attributes;

	std::vector<uint32_t> m_indexBuffer;
	std::vector<glm::mat4> m_instances;

public:
	void addPrimitive(
		std::vector<Vertex>& vertices,
		std::vector<uint32_t>& indices,
		uint32_t& vertexByteOffset,
		uint32_t& indexByteOffset
	);
	void addInstances(std::vector<glm::mat4> instances) {
		m_instances = instances;
	}
	std::pair<uint32_t, uint32_t> getPrimitiveInstances();

	ResourceManager::AllocationIndex buildBuffers(
		ResourceManager& resourceManager
	);
};