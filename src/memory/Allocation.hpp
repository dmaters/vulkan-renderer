#pragma once
#include <cstdint>
#include <forward_list>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Allocation {
	vk::DeviceMemory memory;
	void* address = nullptr;
};

struct SubAllocation {
	uint8_t allocationIndex;
	uint32_t offset;
	size_t size;
	void* address = nullptr;
};

class RingAllocation {
private:
	Allocation m_allocation;
	uint32_t m_occupiedOffset = 0;

public:
	RingAllocation(Allocation allocation);

	inline vk::DeviceMemory getMemory() const { return m_allocation.memory; }

	bool subAllocate(
		SubAllocation& subAllocation, vk::MemoryRequirements requirements
	);
	bool free(SubAllocation& subAllocation) { return true; }
};

class BuddyAllocator {
private:
	static constexpr uint8_t LEVELS = 20;

	Allocation m_allocation;
	std::array<std::forward_list<uint32_t>, LEVELS> m_tree;

public:
	BuddyAllocator(Allocation allocation);

	inline vk::DeviceMemory getMemory() const { return m_allocation.memory; }

	bool subAllocate(
		SubAllocation& subAllocation, vk::MemoryRequirements requirements
	);
	bool free(SubAllocation& subAllocation);
};
