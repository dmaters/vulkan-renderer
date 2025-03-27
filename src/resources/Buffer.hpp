#pragma once

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "memory/MemoryAllocator.hpp"

struct BufferAccess {
	uint32_t length;
	uint32_t offset = 0;
	vk::AccessFlags2 accessType = vk::AccessFlagBits2::eNone;
	vk::PipelineStageFlags2 accessStage =
		vk::PipelineStageFlagBits2::eBottomOfPipe;
};

struct Buffer {
	vk::Buffer buffer;
	SubAllocation allocation;
	size_t size;
	std::vector<BufferAccess> bufferAccess;
	bool transient;
};