#pragma once
#include <cstdint>
#include <forward_list>
#include <map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Allocation {
	Allocation() = delete;
	Allocation(vk::MemoryPropertyFlags type, uint32_t size);
	size_t size = 0;
	vk::DeviceMemory memory;
	std::byte* address = nullptr;
};

struct SubAllocation {
	uint32_t offset = 0;
	size_t size = 0;
};
