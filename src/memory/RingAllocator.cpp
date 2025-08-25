#include "RingAllocator.hpp"

#include "Instance.hpp"
#include "memory/Allocation.hpp"

RingAllocator::RingAllocator(
	vk::MemoryPropertyFlags memoryType, uint32_t size
) :
	m_allocation(memoryType, size) {}

SubAllocation RingAllocator::subAllocate(vk::MemoryRequirements requirements) {
	uint32_t startingOffset = m_occupiedOffset;
	if (requirements.alignment > 0)
		startingOffset += startingOffset % requirements.alignment;

	if (m_allocation.size - startingOffset < requirements.size)
		startingOffset = 0;

	SubAllocation subAllocation = {
		.offset = startingOffset,
		.size = requirements.size,
	};

	m_occupiedOffset = startingOffset + requirements.size;
	return subAllocation;
}

RingAllocator::~RingAllocator() {
	Instance::Get().device.freeMemory(m_allocation.memory);
}