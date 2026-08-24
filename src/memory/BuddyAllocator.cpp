#include "BuddyAllocator.hpp"

#include <cmath>
#include <optional>
#include <vulkan/vulkan_enums.hpp>

#include "Allocation.hpp"

BuddyAllocator::BuddyAllocator(vk::MemoryPropertyFlags memory, uint32_t size, uint32_t minimumAlignment) :
	m_allocation(memory, size) {
	uint32_t levelsRequired = std::log2(size) - std::log2(minimumAlignment);

	m_levels = std::min(20u, levelsRequired);
	m_minAlignment = std::max(minimumAlignment, (uint32_t)(1 << std::max(m_levels - 20, 1)));
	m_baseLevel = std::log2(m_minAlignment);

	m_tree.reserve(m_levels);
	m_tree.at(m_levels - 1).push_front({ 0 });
}

uint32_t BuddyAllocator::getRoundedSize(uint32_t size) {
	if (size < m_minAlignment) return m_minAlignment;
	return std::pow(2, std::ceil(std::log2(size)));
}

std::optional<SubAllocation> BuddyAllocator::subAllocate(vk::MemoryRequirements requirements) {
	uint32_t minBlockSize = std::max(getRoundedSize(requirements.size), (uint32_t)requirements.alignment);
	uint8_t baseLevel = getLevel(minBlockSize);
	uint8_t currentLevel = baseLevel;

	while (m_tree[currentLevel].empty()) {
		currentLevel += 1;
		if (currentLevel >= m_levels) return std::nullopt;
	}

	uint32_t offset = m_tree[currentLevel].front();

	while (currentLevel > baseLevel) {
		m_tree[currentLevel].pop_front();
		m_tree[currentLevel - 1].push_front(offset + getSizeFromLevel(currentLevel - 1));
		m_tree[currentLevel - 1].push_front(offset);

		currentLevel -= 1;
	}

	m_tree[currentLevel].pop_front();

	assert(offset % m_minAlignment == 0 && (requirements.alignment == 0 || offset % requirements.alignment == 0));

	return SubAllocation {
		.offset = offset,
		.size = requirements.size,
	};
}

void BuddyAllocator::free(SubAllocation& subAllocation) {
	uint32_t size = getRoundedSize(subAllocation.size);
	uint8_t level = getLevel(size);
	uint32_t buddy = subAllocation.offset ^ size;

	std::forward_list<uint32_t>& freeList = m_tree[level];

	auto buddyPosition = std::find(freeList.begin(), freeList.end(), buddy);
	if (buddyPosition != freeList.end()) {
		freeList.remove(buddy);
		if (level != m_baseLevel - 1) m_tree[level + 1].push_front(std::min(subAllocation.offset, buddy));
	} else {
		m_tree[level].push_front(subAllocation.offset);
	}
}
