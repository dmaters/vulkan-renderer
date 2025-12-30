#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

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

struct PrimitiveManager {
	std::vector<glm::vec3> m_positions;
	std::vector<Vertex::Attributes> m_attributes;

	std::vector<uint32_t> m_indexBuffer;
	std::vector<glm::mat4> m_instances;

	void addPrimitive(
		std::vector<Vertex>& vertices,
		std::vector<uint32_t>& indices,
		uint32_t& vertexByteOffset,
		uint32_t& indexByteOffset
	);
	void addInstances(std::vector<glm::mat4> instances) {
		m_instances = instances;
	}
};
