#pragma once
#include <optional>
#include <cmath>
#include <vulkan/vulkan.hpp>

#include "Allocation.hpp"

class BuddyAllocator {
private:
	Allocation m_allocation;

	uint8_t m_levels;
	uint8_t m_baseLevel;

	uint32_t m_minAlignment;
	std::vector<std::forward_list<uint32_t>> m_tree;

	uint32_t getRoundedSize(uint32_t size);
	uint32_t getSizeFromLevel(uint8_t level) {
		return 1 << (level + m_baseLevel);
	}
	uint8_t getLevel(uint32_t size) {
		return std::max(m_baseLevel, (uint8_t)std::log2(size)) - m_baseLevel;
	}

public:
	BuddyAllocator(
		vk::MemoryPropertyFlags, uint32_t size, uint32_t minAlignment
	);
	Allocation& getAllocation() { return m_allocation; }
	std::optional<SubAllocation> subAllocate(vk::MemoryRequirements requirements
	);

	void free(SubAllocation& subAllocation);
};
