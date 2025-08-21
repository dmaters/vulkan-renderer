#include "LinearAllocator.hpp"

#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "memory/Allocation.hpp"

LinearAllocator::LinearAllocator(
	vk::MemoryPropertyFlags memory, uint32_t size
) :
	m_allocation(memory, size) {}

SubAllocation LinearAllocator::subAllocate(vk::MemoryRequirements requirements
) {
	uint32_t offsetAligment = requirements.alignment > 0
	                              ? m_occupiedOffset % requirements.alignment
	                              : 0;
	assert((offsetAligment + requirements.size) <= m_allocation.size);

	m_occupiedOffset += offsetAligment;

	return {
		.offset = m_occupiedOffset,
		.size = requirements.size,
	};
}

LinearAllocator::~LinearAllocator() {
	Instance::Get().device.free(m_allocation.memory);
}