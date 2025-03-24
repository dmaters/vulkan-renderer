#include "Allocation.hpp"

#include <vulkan/vulkan_structs.hpp>

RingAllocation::RingAllocation(Allocation allocation) :
	m_allocation(allocation) {}

bool RingAllocation::subAllocate(
	SubAllocation& subAllocation, vk::MemoryRequirements requirements
) {
	uint32_t startingOffset = 0;
	if (m_occupiedOffset > 0) {
		startingOffset =
			m_occupiedOffset + (requirements.alignment -
		                        m_occupiedOffset % requirements.alignment);
	}

	if ((256ull << 20) - startingOffset < requirements.size) startingOffset = 0;

	subAllocation = {
		.offset = startingOffset,
		.size = requirements.size,
		.address = m_allocation.address == nullptr
		               ? nullptr
		               : (char*)m_allocation.address + startingOffset,
	};

	m_occupiedOffset = startingOffset + requirements.size;
	return true;
}