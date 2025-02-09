#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "MemoryAllocator.hpp"

struct Image {
	vk::Image image = nullptr;
	vk::ImageView view = nullptr;
	std::optional<MemoryAllocator::ImageAllocation> allocation;
	vk::Format format;
	vk::ImageLayout layout = vk::ImageLayout::eUndefined;
	void layoutTransition(
		vk::CommandBuffer& commandBuffer, vk::ImageLayout newLayout
	);

	static void LayoutTransition(
		vk::CommandBuffer& commandBuffer,
		vk::Image& image,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout
	);
};
