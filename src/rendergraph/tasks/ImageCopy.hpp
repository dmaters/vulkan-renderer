#pragma once

#include <string_view>

#include "../RenderGraph.hpp"
#include "../RenderGraphBuilder.hpp"
#include "Task.hpp"

class ImageCopy : public Task {
public:
private:
	std::string_view m_origin;
	std::string_view m_destination;

public:
	void setup(
		std::vector<ImageDependencyInfo>& requiredImages,
		std::vector<BufferDependencyInfo>& requiredBuffers
	) override;
	void execute(vk::CommandBuffer& commandBuffer, const Resources& resources)
		override;

	ImageCopy(std::string_view origin, std::string_view destination) :
		m_origin(origin), m_destination(destination) {}
};