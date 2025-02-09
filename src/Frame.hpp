#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

struct Frame {
	vk::CommandPool commandPool;
	vk::Fence fence;
	vk::Semaphore imageAvailable;
	vk::Semaphore renderFinished;
};
