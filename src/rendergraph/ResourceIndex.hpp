#pragma once
#include <cassert>
#include <cstdint>

namespace rendergraph {
enum class ResourceType {
	Image = 0x0,
	Buffer = 0x1
};
using ResourceIndex = uint32_t;

};	// namespace rendergraph
