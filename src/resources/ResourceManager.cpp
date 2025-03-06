#include "ResourceManager.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Buffer.hpp"
#include "MemoryAllocator.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>

ResourceManager::ResourceManager(Instance &instance) :
	m_device(instance.device), m_memoryAllocator(instance) {}

Buffer ResourceManager::createBuffer(const BufferDescription &description) {
	vk::BufferCreateInfo createInfo {
		.size = description.size,
		.usage = description.usage,
	};

	MemoryAllocator::BufferAllocation allocation =
		m_memoryAllocator.allocateBuffer(createInfo, description.location);
	return {
		.buffer = allocation.buffer,
		.allocation = allocation,
		.size = description.size,
	};
}
Image ResourceManager::createImage(const ImageDescription &description) {
	const vk::ImageCreateInfo imageInfo {
		.flags = {},

		.imageType = vk::ImageType::e2D,
		.format = description.format,
		.extent = vk::Extent3D(description.width, description.height, 1),
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.usage = description.usage,
		.initialLayout = vk::ImageLayout::eUndefined,
	};
	MemoryAllocator::ImageAllocation allocation =
		m_memoryAllocator.allocateImage(imageInfo);

	vk::ImageViewCreateInfo viewInfo {
		.image = allocation.image,
		.viewType = vk::ImageViewType::e2D,
		.format = description.format,
		.subresourceRange = { .aspectMask =
		                          description.format == vk::Format::eD16Unorm
		                              ? vk::ImageAspectFlagBits::eDepth
		                              : vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,},
	};

	return Image {
		.image = allocation.image,
		.view = m_device.createImageView(viewInfo),
		.format = description.format,
		.layout = vk::ImageLayout::eUndefined,
		.size = { .width = description.width,
                 .height = description.height,
                 .depth = 1, },
		            .allocation = allocation,
	};
}

struct ImageData {
	uint32_t x;
	uint32_t y;
	uint8_t channels;
	std::vector<std::byte> data;
};

ImageData load(const std::filesystem::path &image) {
	assert(!image.empty());

	int x, y, channels;
	unsigned char *rawData =
		stbi_load(image.string().c_str(), &x, &y, &channels, 4);
	if (rawData == nullptr && stbi_failure_reason()) {
		std::cout << "Error loading " + image.string() + "|"
				  << "Failed for error " << stbi_failure_reason() << std::endl;
		throw "Error loading image";
	}
	size_t size = x * y * channels * sizeof(std::byte);
	std::vector<std::byte> vectorData(size);
	memcpy(vectorData.data(), rawData, size);
	stbi_image_free(rawData);

	return { .x = (uint32_t)x,
		     .y = (uint32_t)y,
		     .channels = (uint8_t)channels,
		     .data = vectorData };
}
Image ResourceManager::loadImage(const std::filesystem::path &path) {
	ImageData data = load(path);
	ImageDescription description {
		.width = data.x,
		.height = data.y,
		.format = vk::Format::eR8G8B8A8Srgb,
		.usage = vk::ImageUsageFlagBits::eTransferDst |
		         vk::ImageUsageFlagBits::eSampled,

	};

	Image image = createImage(description);
	MemoryAllocator::BufferAllocation staging =
		m_memoryAllocator.allocateStagingBuffer(data.data.size());

	m_memoryAllocator.copyToBuffer(data.data, staging);

	m_memoryAllocator.copyToImage(staging, image.allocation.value(), {
		.bufferOffset = 0,
		.bufferRowLength = data.x,
		.bufferImageHeight = data.y,
		.imageSubresource = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0,0,0},
		.imageExtent = {
			.width = data.x,
			.height = data.y,
			.depth = 1
		},
		
		
	});

	image.layout = vk::ImageLayout::eShaderReadOnlyOptimal;

	m_memoryAllocator.freeAllocation(staging.allocation);
	return image;
}