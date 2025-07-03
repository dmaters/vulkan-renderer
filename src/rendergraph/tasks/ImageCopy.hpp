#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

struct Resources;
struct ResourceDependency;

class ImageCopy {
public:
private:
	std::string_view m_origin;
	std::string_view m_destination;

public:
	void setup(
		std::unordered_map<std::string_view, ResourceDependency>& images,
		std::unordered_map<std::string_view, ResourceDependency>& buffers
	);
	void execute(vk::CommandBuffer& commandBuffer, const Resources& resources);

	ImageCopy(std::string_view origin, std::string_view destination) :
		m_origin(origin), m_destination(destination) {}
};