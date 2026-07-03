#pragma once
#include <cassert>
#include <cstdint>

namespace rendergraph {

using ResourceIndex = uint32_t;
using ImageIndex = uint32_t;
using BufferIndex = uint32_t;

};  // namespace rendergraph

namespace rendergraph::internal {

struct ResourceIndexer {
	std::size_t imageCount = 0;
	std::size_t bufferCount = 0;

	enum class ResourceType {
		Image = 0x0,
		Buffer = 0x1
	};

	static inline ResourceIndex From(uint32_t index, ResourceType type) {
		return index << 1 | static_cast<uint32_t>(type);
	}
	static ResourceType ResourceIndex_getType(ResourceIndex index) {
		return (ResourceType)(index & 0x1);
	}

	uint32_t To(ResourceIndex index) { return index >> 1; }

	ResourceIndex registerResource(ResourceType type) {
		if (type == ResourceType::Image) {
			ResourceIndex index = From(imageCount, ResourceType::Image);
			imageCount += 1;
			return index;
		} else if (type == ResourceType::Buffer) {
			ResourceIndex index = From(imageCount, ResourceType::Buffer);
			bufferCount += 1;
			return index;
		}
	}

	std::size_t count() { return imageCount + bufferCount; }
};
};  // namespace rendergraph::internal
