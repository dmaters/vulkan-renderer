#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "memory/MemoryAllocator.hpp"

struct ImageAccess {
	vk::ImageView view;
	vk::ImageLayout layout;
	vk::AccessFlags2 accessType;
	vk::PipelineStageFlags2 accessStage;
};

struct Image {
	vk::Image image = nullptr;
	vk::ImageView view = {};
	vk::Format format;
	vk::Extent3D size;
	std::optional<SubAllocation> allocation;

	vk::ImageAspectFlags getAspectFlags() {
		vk::ImageAspectFlags flags;
		if (format == vk::Format::eD16Unorm)
			flags = vk::ImageAspectFlagBits::eDepth;
		else
			flags = vk::ImageAspectFlagBits::eColor;
		return flags;
	}
	std::vector<ImageAccess> accesses;
	bool transient = false;
};
