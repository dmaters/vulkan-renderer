#pragma once

#include <vk_mem_alloc.h>

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "MemoryAllocator.hpp"

struct Buffer {
	vk::Buffer buffer;
	MemoryAllocator::BufferAllocation allocation;
	size_t size;
};