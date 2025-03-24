#pragma once

#include <cstddef>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "memory/MemoryAllocator.hpp"

struct Buffer {
	vk::Buffer buffer;
	SubAllocation allocation;
	size_t size;
};