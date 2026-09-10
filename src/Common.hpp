#pragma once
#include <cstddef>

struct MemorySpan {
	std::size_t size = 0;
	std::size_t offset = 0;
};
