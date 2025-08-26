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
	uint32_t offsetAligment =
		requirements.alignment > 1
			? (requirements.alignment -
	           (m_occupiedOffset % requirements.alignment))
			: 0;
	assert((offsetAligment + requirements.size) <= m_allocation.size);

	uint32_t baseOffset = m_occupiedOffset + offsetAligment;
	m_occupiedOffset += offsetAligment + requirements.size;
	return {
		.offset = baseOffset,
		.size = requirements.size,
	};
}

LinearAllocator::~LinearAllocator() {
	Instance::Get().device.free(m_allocation.memory);
}
