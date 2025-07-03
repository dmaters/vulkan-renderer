#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "memory/MemoryAllocator.hpp"

struct ImageAccess {
	vk::ImageView view;
	vk::ImageLayout layout;
};

struct Image {
	vk::Image image = nullptr;
	vk::ImageView view = {};
	vk::Format format;
	vk::Extent3D size;
	std::optional<SubAllocation> allocation;
	std::vector<ImageAccess> accesses;
	bool transient = false;

	vk::ImageAspectFlags getAspectFlags() const {
		vk::ImageAspectFlags flags;
		if (format == vk::Format::eD16Unorm)
			flags = vk::ImageAspectFlagBits::eDepth;
		else
			flags = vk::ImageAspectFlagBits::eColor;
		return flags;
	}
};
