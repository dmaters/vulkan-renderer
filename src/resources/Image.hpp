#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "memory/MemoryAllocator.hpp"

struct Image {
	vk::Image image = nullptr;
	vk::ImageView view = nullptr;
	vk::Format format;
	vk::ImageLayout layout = vk::ImageLayout::eUndefined;
	vk::Extent3D size;

	std::optional<MemoryAllocator::SubAllocation> allocation;

	vk::ImageAspectFlags getAspectFlags() {
		vk::ImageAspectFlags flags;
		if (format == vk::Format::eD16Unorm)
			flags = vk::ImageAspectFlagBits::eDepth;
		else
			flags = vk::ImageAspectFlagBits::eColor;

		return flags;
	}
};
