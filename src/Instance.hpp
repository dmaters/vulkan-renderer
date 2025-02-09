#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

struct Instance {
	struct QueueFamilies {
		uint32_t graphicsIndex;
		uint32_t transferIndex;
		uint32_t presentIndex;
	};
	vk::Device device;
	vk::PhysicalDevice physicalDevice;
	vk::Instance instance;

	QueueFamilies queueFamiliesIndices;
};
