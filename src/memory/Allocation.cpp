#include "Allocation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <list>
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
BuddyAllocator::BuddyAllocator(Allocation allocation) :
	m_allocation(allocation) {
	m_tree[LEVELS - 1] = { 0 };
}

uint32_t getRoundedSize(uint32_t size) {
	if (size < 1024) return 1024;
	return std::pow(2, std::ceil(std::log2(size)));
}

constexpr uint32_t getSizeFromLevel(uint8_t level) { return 1 << (level + 10); }

uint8_t getLevel(uint32_t size) { return std::log2(size) - std::log2(1024); }

bool BuddyAllocator::subAllocate(
	SubAllocation& subAllocation, vk::MemoryRequirements requirements
) {
	uint8_t baseLevel = getLevel(getRoundedSize(requirements.size));
	uint8_t currentLevel = baseLevel;

	while (m_tree[currentLevel].empty()) {
		currentLevel += 1;
		if (currentLevel > LEVELS) return false;
	}

	while (currentLevel > baseLevel) {
		uint32_t address = m_tree[currentLevel].front();
		m_tree[currentLevel].pop_front();

		m_tree[currentLevel - 1].push_front(
			address + getSizeFromLevel(currentLevel - 1)
		);
		m_tree[currentLevel - 1].push_front(address);

		currentLevel -= 1;
	}

	uint32_t offset = m_tree[currentLevel].front();

	assert(offset % 1024 == 0);
	m_tree[currentLevel].pop_front();

	subAllocation = {
		.offset = offset,
		.size = requirements.size,
		.address = m_allocation.address != nullptr
		               ? (char*)m_allocation.address + offset
		               : nullptr,

	};

	return true;
}

bool BuddyAllocator::free(SubAllocation& subAllocation) {
	uint32_t size = getRoundedSize(subAllocation.size);
	uint8_t level = getLevel(size);
	uint32_t buddy = subAllocation.offset ^ size;

	std::forward_list<uint32_t>& freeList = m_tree[level];

	auto buddyPosition = std::find(freeList.begin(), freeList.end(), buddy);
	if (buddyPosition != freeList.end()) {
		freeList.remove(buddy);
		if (level != LEVELS - 1)
			m_tree[level + 1].push_front(std::min(subAllocation.offset, buddy));
	} else {
		m_tree[level].push_front(subAllocation.offset);
	}

	return true;
}
