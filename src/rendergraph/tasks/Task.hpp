#pragma once

#include <vulkan/vulkan.hpp>

#include "../RenderGraphBuilder.hpp"

struct Resources;
class Task {
public:
	virtual void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) = 0;
	virtual void execute(
		vk::CommandBuffer& buffer, const Resources& resources
	) = 0;
};