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

BufferHandle ResourceManager::createBuffer(const BufferDescription &description
) {
	vk::BufferCreateInfo createInfo {
		.size = description.transient ? description.size * 3 : description.size,
		.usage = description.usage,
	};

	vk::Buffer buffer = m_device.createBuffer(createInfo);

	SubAllocation allocation = m_memoryAllocator.allocate(
		buffer, AllocationType::Persistent, description.location
	);

	BufferHandle handle { m_resourceCounter };
	m_resourceCounter++;

	m_buffers[handle.value] = {
		.buffer = buffer,
		.allocation = allocation,
		.size = description.size,
		.bufferAccess = std::vector<BufferAccess> { BufferAccess {
			.length = description.size,
			.offset = 0,
		} },
		.transient = description.transient,
	};

	if (description.transient) {
		m_buffers[handle.value].bufferAccess.push_back({
			.length = description.size,
			.offset = description.size,
		});
		m_buffers[handle.value].bufferAccess.push_back({
			.length = description.size,
			.offset = description.size * 2,
		});
	}

	return handle;
}
ImageHandle ResourceManager::createImage(const ImageDescription &description) {
	vk::ImageCreateInfo imageInfo {
		.flags = {},
		.imageType = description.depth > 1 ? vk::ImageType::e3D
		                                   : vk::ImageType::e2D,
		.format = description.format,
		.extent = vk::Extent3D(
			description.width, description.height, description.depth
		),
		.mipLevels = 1,
		.arrayLayers = description.transient ? 3u : 1u,
		.samples = vk::SampleCountFlagBits::e1,
		.usage = description.usage,
		.initialLayout = vk::ImageLayout::eUndefined,
	};

	vk::Image image = m_device.createImage(imageInfo);

	SubAllocation allocation = m_memoryAllocator.allocate(
		image, AllocationType::Persistent, AllocationLocation::Device
	);

	std::vector<vk::ImageView> views;

	vk::ImageViewCreateInfo viewInfo {
		.image = image,
		.viewType = description.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
		.format = description.format,
		.subresourceRange = { .aspectMask =
		                          description.format == vk::Format::eD16Unorm
		                              ? vk::ImageAspectFlagBits::eDepth
		                              : vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1,
							},
	};

	views.push_back(m_device.createImageView(viewInfo));

	if (description.transient) {
		viewInfo.subresourceRange.baseArrayLayer = 1;
		views.push_back(m_device.createImageView(viewInfo));
		viewInfo.subresourceRange.baseArrayLayer = 2;
		views.push_back(m_device.createImageView(viewInfo));
	}

	ImageHandle handle { m_resourceCounter };
	m_resourceCounter++;

	Image finalImage {
		.image = image,
		.view = views[0],
		.format = description.format,
		.size = { .width = description.width,
                 .height = description.height,
                 .depth = description.depth,
				},
		.allocation = allocation,
		.accesses = std::vector<ImageAccess>(!description.transient ?1 : 3, ImageAccess{
			.view = views[0],
			.layout = vk::ImageLayout::eUndefined,
			.accessType = vk::AccessFlagBits2::eNone,
			.accessStage = vk::PipelineStageFlagBits2::eNone,
		}),
		.transient = description.transient
	};

	if (description.transient) {
		finalImage.accesses[1] = { .view = views[1] };
		finalImage.accesses[2] = { .view = views[2] };
	}
	m_images[handle.value] = finalImage;
	return handle;
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

	return {
		.x = (uint32_t)x,
		.y = (uint32_t)y,
		.channels = (uint8_t)channels,
		.data = vectorData,
	};
}
ImageHandle ResourceManager::loadImage(const std::filesystem::path &path) {
	ImageData data = load(path);
	ImageDescription description {
		.width = data.x,
		.height = data.y,
		.format = vk::Format::eR8G8B8A8Srgb,
		.usage = vk::ImageUsageFlagBits::eTransferDst |
		         vk::ImageUsageFlagBits::eSampled,

	};

	ImageHandle image = createImage(description);

	BufferHandle staging = createStagingBuffer(data.data.size());

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

	free(staging);
	return image;
}

BufferHandle ResourceManager::createStagingBuffer(uint32_t size) {
	vk::BufferCreateInfo info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	};
	vk::Buffer buffer = m_device.createBuffer(info);
	SubAllocation allocation = m_memoryAllocator.allocate(
		buffer, AllocationType::Staging, AllocationLocation::Host
	);
	BufferHandle handle { m_resourceCounter };
	m_resourceCounter++;

	m_buffers[handle.value] = Buffer { .buffer = buffer,
		                               .allocation = allocation,
		                               .size = size,
		                               .bufferAccess = { BufferAccess {
										   .length = size,
										   .offset = 0,
									   }, }, };

	return handle;
}
ImageHandle ResourceManager::registerImage(Image image) {
	ImageHandle handle { m_resourceCounter };
	m_resourceCounter++;

	m_images[handle.value] = image;
	return handle;
}
void ResourceManager::copyBuffer(
	BufferHandle origin, BufferHandle destination, vk::BufferCopy offset
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

	commandBuffer.copyBuffer(
		m_buffers[origin.value].buffer,
		m_buffers[destination.value].buffer,
		offset
	);
	commandBuffer.end();
	vk::SubmitInfo submitInfo { .commandBufferCount = 1,
		                        .pCommandBuffers = &commandBuffer };

	m_queue.submit({ submitInfo });
};
void ResourceManager::copyToImage(
	BufferHandle origin, ImageHandle destination, vk::BufferImageCopy offset
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
        .image = m_images[destination.value].image,
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
		m_buffers[origin.value].buffer,
		m_images[destination.value].image,
		vk::ImageLayout::eTransferDstOptimal,
		{ offset }
	);
	barrier = {
        .srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = m_images[destination.value].image,
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
	const std::vector<std::byte> &data, BufferHandle handle
) {
	Buffer &buffer = m_buffers[handle.value];
	if (buffer.allocation.address != nullptr) {
		memcpy((char *)buffer.allocation.address, data.data(), data.size());
		return;
	}

	BufferHandle staging = createStagingBuffer(data.size());
	std::memcpy(
		(char *)m_buffers[staging.value].allocation.address,
		data.data(),
		data.size()
	);

	copyBuffer(staging, handle, { .size = data.size() });
	free(staging);
}
