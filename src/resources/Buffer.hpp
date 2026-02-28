#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "memory/Allocation.hpp"

struct Buffer {
	vk::Buffer buffer;
	SubAllocation allocation;
	size_t size;
	void* data;
};
