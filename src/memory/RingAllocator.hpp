#pragma once
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "Allocation.hpp"

class RingAllocator {
private:
	Allocation m_allocation;
	uint32_t m_occupiedOffset = 0;

public:
	RingAllocator(vk::MemoryPropertyFlags memoryType, uint32_t size);
	RingAllocator(const RingAllocator&) = delete;

	SubAllocation subAllocate(vk::MemoryRequirements requirements);

	Allocation& getAllocation() { return m_allocation; }

	~RingAllocator();
};
