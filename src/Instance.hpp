#pragma once

#include <cstdint>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Swapchain.hpp"

struct SDL_Window;
struct Instance {
public:
	struct QueueFamilies {
		uint32_t graphicsIndex = 0;
		uint32_t transferIndex = 0;
		uint32_t presentIndex = 0;

		bool transferAvailable = false;
	};
	vk::Device device;
	vk::SurfaceKHR surface;
	vk::SurfaceFormatKHR surfaceFormat;

	vk::PhysicalDevice physicalDevice;
	struct PhysicalDeviceLimits {
		std::size_t minStorageBufferOffsetAlignment;
		std::size_t minUniformBufferOffsetAlignment;
	};
	PhysicalDeviceLimits physicalDeviceLimits;
	vk::Instance instance;

	vk::Queue graphicQueue;
	vk::Queue transferQueue;
	vk::Queue presentQueue;

	Swapchain swapchain;

	QueueFamilies queueFamiliesIndices;
	static Instance& Create(SDL_Window* window);
	static Instance& Get() { return _instance.value(); }

private:
	static std::optional<Instance> _instance;
};
