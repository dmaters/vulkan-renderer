#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

struct SDL_Window;
struct Instance {
	struct QueueFamilies {
		uint32_t graphicsIndex = 0;
		uint32_t transferIndex = 0;
		uint32_t presentIndex = 0;
	};
	vk::Device device;
	vk::SurfaceKHR surface;
	vk::PhysicalDevice physicalDevice;
	vk::Instance instance;

	QueueFamilies queueFamiliesIndices;

	static Instance Create(SDL_Window* window);
};
