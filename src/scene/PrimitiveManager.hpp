#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Primitive.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

class PrimitiveManager {
private:
	std::vector<std::byte> m_vertexbuffer;
	std::vector<std::byte> m_indexBuffer;

public:
	void addPrimitive(
		std::vector<std::byte> vertices,
		std::vector<std::byte> indices,
		uint32_t& vertexByteOffset,
		uint32_t& indexByteOffset
	);
	bool buildBuffers(ResourceManager& resourceManager);
};