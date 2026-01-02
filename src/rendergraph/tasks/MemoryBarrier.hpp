#pragma once
#include <vulkan/vulkan.hpp>

#include "Task.hpp"

void MemoryBarrier(
	TaskContext& context,
	std::unordered_map<ResourceIndex,vk::BufferMemoryBarrier2> bufferResourceBarriers,
	std::unordered_map<ResourceIndex,vk::ImageMemoryBarrier2> imageResourceBarriers
	);


