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

ResourceManager::BufferHandle ResourceManager::createBuffer(size_t size) {
	vk::BufferCreateInfo createInfo {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eUniformBuffer,
	};

	BufferHandle handle(*this);
	MemoryAllocator::BufferAllocation allocation =
		m_memoryAllocator.allocateBuffer(
			createInfo, MemoryAllocator::Location::HostMapped
		);
	Buffer buffer {
		.buffer = allocation.buffer,
		.allocation = allocation,
		.size = size,
	};
	m_buffers.insert({ handle, buffer });
	return handle;
}

ResourceManager::ImageHandle ResourceManager::createImage(
	const ImageDescription &description
) {
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
		.subresourceRange = vk::ImageSubresourceRange(
			vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
		),
	};

	ImageHandle handle = ImageHandle(*this);

	m_images.insert({
		handle,
		Image {
			   .image = allocation.image,
			   .view = m_device.createImageView(viewInfo),
			   .allocation = allocation,
			   .format = description.format,
			   .layout = vk::ImageLayout::eUndefined,
			   }
    });

	return handle;

	// GESTITA LA CREAZIONE DELLIMMAGINE, GESTISCI IL CARICAMENTO DA LOCALE
}

struct ImageData {
	uint32_t x;
	uint32_t y;
	uint8_t channels;
	std::vector<unsigned char> data;
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
	size_t size = x * y * channels * sizeof(uint8_t);
	std::vector<unsigned char> vectorData(size);
	memcpy(vectorData.data(), rawData, size);
	stbi_image_free(rawData);

	return { .x = (uint32_t)x,
		     .y = (uint32_t)y,
		     .channels = (uint8_t)channels,
		     .data = vectorData };
}
ResourceManager::ImageHandle ResourceManager::loadImage(
	const std::filesystem::path &path
) {
	if (m_imageCache.find(path) != m_imageCache.end())
		return m_imageCache.at(path);

	ImageData data = load(path);
	ImageDescription description {
		.width = data.x,
		.height = data.y,
		.format = vk::Format::eR8G8B8A8Srgb,
		.usage = vk::ImageUsageFlagBits::eTransferDst |
		         vk::ImageUsageFlagBits::eSampled,

	};

	ImageHandle image = createImage(description);
	MemoryAllocator::BufferAllocation staging =
		m_memoryAllocator.allocateStagingBuffer(data.data.size());

	staging.updateData(data.data);

	m_memoryAllocator.copyToImage(staging, m_images.at(image).allocation.value(), {
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

	m_images[image].layout = vk::ImageLayout::eShaderReadOnlyOptimal;

	m_memoryAllocator.freeAllocation(staging.base);
	m_imageCache.insert({ path, image });
	return image;
}