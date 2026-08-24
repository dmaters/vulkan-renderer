#include "LinearAllocator.hpp"

#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "memory/Allocation.hpp"

LinearAllocator::LinearAllocator(vk::MemoryPropertyFlags memory, uint32_t size) : m_allocation(memory, size) {}

SubAllocation LinearAllocator::subAllocate(vk::MemoryRequirements requirements) {
	uint32_t alignedOffset = (m_occupiedOffset + requirements.alignment - 1) & ~(requirements.alignment - 1);
	assert((alignedOffset + requirements.size) <= m_allocation.size);

	m_occupiedOffset = alignedOffset + requirements.size;
	return {
		.offset = alignedOffset,
		.size = requirements.size,
	};
}
