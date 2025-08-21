#pragma once

#include <vulkan/vulkan.hpp>

#include "Allocation.hpp"

class LinearAllocator {
private:
	Allocation m_allocation;
	uint32_t m_occupiedOffset = 0;

public:
	LinearAllocator(vk::MemoryPropertyFlags memory, uint32_t size);
	LinearAllocator(const LinearAllocator&) = delete;

	Allocation& getAllocation() { return m_allocation; }

	SubAllocation subAllocate(vk::MemoryRequirements requirements);

	~LinearAllocator();
};
