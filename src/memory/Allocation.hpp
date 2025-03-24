#pragma once
#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Allocation {
	vk::DeviceMemory memory;
	void* address = nullptr;
};
struct SubAllocation {
	uint8_t allocationIndex = 0;
	uint64_t offset;
	size_t size;
	void* address = nullptr;
};

class RingAllocation {
private:
	Allocation m_allocation;
	uint64_t m_occupiedOffset = 0;

public:
	RingAllocation(Allocation);

	inline vk::DeviceMemory getMemory() const { return m_allocation.memory; }

	bool subAllocate(
		SubAllocation& subAllocation, vk::MemoryRequirements requirements
	);
};