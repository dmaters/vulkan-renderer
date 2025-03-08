#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "memory/MemoryAllocator.hpp"

struct Buffer {
	vk::Buffer buffer;
	MemoryAllocator::SubAllocation allocation;
	size_t size;
};