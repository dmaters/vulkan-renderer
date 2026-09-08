#pragma once
#include <cstddef>

struct MemorySpan {
	std::size_t size;
	std::size_t offset;
};
