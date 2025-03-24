#include "ResourceManager.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Buffer.hpp"
#include "Image.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/Buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>

ResourceManager::ResourceManager(
	Instance &instance, MemoryAllocator &memoryAllocator
) :
	m_device(instance.device), m_memoryAllocator(memoryAllocator) {
	m_queue = instance.device.getQueue(
		instance.queueFamiliesIndices.transferIndex, 0
	);

	m_commandPool = m_device.createCommandPool(vk::CommandPoolCreateInfo {
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = instance.queueFamiliesIndices.transferIndex,
	});
}

Buffer ResourceManager::createBuffer(const BufferDescription &description) {
	vk::BufferCreateInfo createInfo {
		.size = description.size,
		.usage = description.usage,

	};

	vk::Buffer buffer = m_device.createBuffer(createInfo);

	SubAllocation allocation = m_memoryAllocator.allocate(
		buffer, AllocationType::Persistent, description.location
	);

	return {
		.buffer = buffer,
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

	vk::Image image = m_device.createImage(imageInfo);

	SubAllocation allocation = m_memoryAllocator.allocate(
		image, AllocationType::Persistent, AllocationLocation::Device
	);

	vk::ImageViewCreateInfo viewInfo {
		.image = image,
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
		.image = image,
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

	Buffer staging = createStagingBuffer(data.data.size());

	copyToBuffer(data.data, staging);

	copyToImage(staging, image, {
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

	free(staging);
	return image;
}

Buffer ResourceManager::createStagingBuffer(size_t size) {
	vk::BufferCreateInfo info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	};
	vk::Buffer buffer = m_device.createBuffer(info);
	SubAllocation allocation = m_memoryAllocator.allocate(
		buffer, AllocationType::Staging, AllocationLocation::Host
	);

	return {
		.buffer = buffer,
		.allocation = allocation,
		.size = size,
	};
}

void ResourceManager::copyBuffer(
	Buffer &origin, Buffer &destination, vk::BufferCopy offset
) {
	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};
	vk::CommandBuffer commandBuffer =
		m_device.allocateCommandBuffers(commandBufferInfo)[0];

	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);

	commandBuffer.copyBuffer(origin.buffer, destination.buffer, offset);
	commandBuffer.end();
	vk::SubmitInfo submitInfo { .commandBufferCount = 1,
		                        .pCommandBuffers = &commandBuffer };

	m_queue.submit({ submitInfo });
};
void ResourceManager::copyToImage(
	Buffer &origin, Image &destination, vk::BufferImageCopy offset
) {
	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};

	vk::CommandBuffer commandBuffer =
		m_device.allocateCommandBuffers(commandBufferInfo)[0];

	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);

	vk::ImageMemoryBarrier2 barrier {
        .dstStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .image = destination.image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
	commandBuffer.pipelineBarrier2({
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,
	});
	commandBuffer.copyBufferToImage(
		origin.buffer,
		destination.image,
		vk::ImageLayout::eTransferDstOptimal,
		{ offset }
	);
	barrier = {
        .srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = destination.image,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
	commandBuffer.pipelineBarrier2({
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier,

	});
	commandBuffer.end();
	vk::SubmitInfo submitInfo {
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
	};

	m_queue.submit({ submitInfo });
}

void ResourceManager::copyToBuffer(
	const std::vector<std::byte> &data, Buffer &buffer
) {
	if (buffer.allocation.address != nullptr) {
		memcpy((char *)buffer.allocation.address, data.data(), data.size());
		return;
	}

	Buffer staging = createStagingBuffer(data.size());
	std::memcpy((char *)staging.allocation.address, data.data(), data.size());

	copyBuffer(staging, buffer, { .size = data.size() });
	free(staging);
}
