#include "ResourceManager.hpp"

#include <compressonator.h>
#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Buffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "memory/Allocation.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/RingAllocator.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>

ResourceManager::ResourceManager() :
	// 256MB
	m_stagingAllocation(vk::MemoryPropertyFlagBits::eHostCoherent, 1 << 28) {
	Instance &instance = Instance::Get();

	m_commandPool =
		Instance::Get().device.createCommandPool(vk::CommandPoolCreateInfo {
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = instance.queueFamiliesIndices.transferIndex,
		});

	vk::SemaphoreTypeCreateInfo type {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};

	m_semaphore = instance.device.createSemaphore({
		.pNext = &type,
	});
}

void ResourceManager::setName(std::string_view name, ImageHandle handle) {
	Instance::Get().device.setDebugUtilsObjectNameEXT(
		vk::DebugUtilsObjectNameInfoEXT {
			.objectType = vk::ObjectType::eImage,
			.objectHandle = (uint64_t)(VkImage)m_images.at(handle.value).image,
			.pObjectName = name.data(),
		}
	);
	m_imageNames[name] = handle;
}
void ResourceManager::setName(std::string_view name, BufferHandle handle) {
	Instance::Get().device.setDebugUtilsObjectNameEXT(
		vk::DebugUtilsObjectNameInfoEXT {
			.objectType = vk::ObjectType::eBuffer,
			.objectHandle =
				(uint64_t)(VkBuffer)m_buffers.at(handle.value).buffer,
			.pObjectName = name.data(),
		}
	);
	m_bufferNames[name] = handle;
}

ImageHandle ResourceManager::registerImage(Image image, ImageHandle handle) {
	if (handle.value == 0) {
		handle = { ++m_resourceCounter };
	}

	m_images[handle.value] = image;
	return handle;
}

void resetPool(
	vk::Semaphore semaphore, uint64_t expectedValue, vk::CommandPool pool
) {
	vk::Device &device = Instance::Get().device;

	if (device.getSemaphoreCounterValue(semaphore) == expectedValue)
		device.resetCommandPool(pool);
}

void ResourceManager::copyBuffers(std::vector<BufferCopy> &info) {
	Instance &instance = Instance::Get();

	resetPool(m_semaphore, m_transferCount, m_commandPool);

	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};
	vk::CommandBuffer commandBuffer =
		instance.device.allocateCommandBuffers(commandBufferInfo)[0];

	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);
	std::vector<vk::BufferMemoryBarrier2> barriers;

	for (auto &copyInfo : info) {
		commandBuffer.copyBuffer(
			copyInfo.origin, copyInfo.destination, copyInfo.copy
		);

		barriers.push_back({
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.srcQueueFamilyIndex = instance.queueFamiliesIndices.transferIndex,
			.dstQueueFamilyIndex = instance.queueFamiliesIndices.graphicsIndex,
			.buffer = copyInfo.destination,
			.offset = 0,
			.size = copyInfo.copy.size,
		});
	}

	commandBuffer.pipelineBarrier2(vk::DependencyInfo {
		.bufferMemoryBarrierCount = (uint32_t)barriers.size(),
		.pBufferMemoryBarriers = barriers.data(),
	});

	commandBuffer.end();
	vk::TimelineSemaphoreSubmitInfo semaphore {
		.signalSemaphoreValueCount = 1,
		.pSignalSemaphoreValues = &(++m_transferCount),
	};
	vk::SubmitInfo submitInfo {
		.pNext = &semaphore,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_semaphore,
	};

	Instance::Get().transferQueue.submit({ submitInfo });

	Instance::Get().device.waitSemaphores(
		{
			.semaphoreCount = 1,
			.pSemaphores = &m_semaphore,
			.pValues = &m_transferCount,
		},
		UINT32_MAX
	);
};
void ResourceManager::copyToImage(
	vk::Buffer origin, vk::Image destination, vk::BufferImageCopy offset
) {
	resetPool(m_semaphore, m_transferCount, m_commandPool);

	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};

	vk::CommandBuffer commandBuffer =
		Instance::Get().device.allocateCommandBuffers(commandBufferInfo)[0];

	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);

	vk::ImageMemoryBarrier2 barrier {
        .dstStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .image = destination,
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
		origin,
		destination,
		vk::ImageLayout::eTransferDstOptimal,
		{ { offset } }
	);
	barrier = {
        .srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
        .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .image = destination,
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

	vk::TimelineSemaphoreSubmitInfo semaphore {
		.signalSemaphoreValueCount = 1,
		.pSignalSemaphoreValues = &(++m_transferCount),
	};
	vk::SubmitInfo submitInfo {
		.pNext = &semaphore,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_semaphore,
	};

	Instance::Get().transferQueue.submit({ submitInfo });

	uint64_t waitValue = m_transferCount;
	Instance::Get().device.waitSemaphores(
		{
			.semaphoreCount = 1,
			.pSemaphores = &m_semaphore,
			.pValues = &waitValue,
		},
		UINT64_MAX
	);
}

struct ImageData {
	uint32_t x;
	uint32_t y;
	uint8_t channels;
	std::vector<std::byte> data;
};

ImageData load(const std::filesystem::path &image) {
	assert(!image.empty());

	int x, y, _;
	stbi_set_flip_vertically_on_load(1);
	unsigned char *rawData = stbi_load(image.string().c_str(), &x, &y, &_, 4);
	if (rawData == nullptr && stbi_failure_reason()) {
		std::cout << "Error loading " + image.string() + "|"
				  << "Failed for error " << stbi_failure_reason() << std::endl;
		throw "Error loading image";
	}
	size_t size = x * y * 4 * sizeof(std::byte);
	std::vector<std::byte> vectorData(size);
	memcpy(vectorData.data(), rawData, size);
	stbi_image_free(rawData);

	return {
		.x = (uint32_t)x,
		.y = (uint32_t)y,
		.channels = 4,
		.data = vectorData,
	};
}

vk::Image createImage(const ResourceManager::ImageDescription &description) {
	vk::Device &device = Instance::Get().device;

	vk::ImageCreateInfo imageInfo {
		.flags = {},
		.imageType = description.depth > 1 ? vk::ImageType::e3D
		                                   : vk::ImageType::e2D,
		.format = description.format,
		.extent = vk::Extent3D(
			description.width, description.height, description.depth
		),
		.mipLevels = description.miplevels,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.usage = description.usage,
		.initialLayout = vk::ImageLayout::eUndefined,
	};

	vk::Image image = device.createImage(imageInfo);

	return image;
}

ResourceManager::AllocationIndex ResourceManager::loadSceneTextures(
	std::vector<TextureInfo> textures
) {
	vk::Device &device = Instance::Get().device;

	for (const auto &info : textures) {
		assert(std::filesystem::exists(info.path));
	}
	std::vector<ImageDescription> textureDesc;
	for (const auto &info : textures) {
		int32_t x, y, channels;
		stbi_info(info.path.string().c_str(), &x, &y, &channels);

		textureDesc.push_back({
			.width = (uint32_t)x,
			.height = (uint32_t)y,
			.depth = 1,
			.miplevels =
				(uint32_t)std::max(1, ((int32_t)std::log2(std::min(x, y))) - 2),
			.format = info.expectedFormat,
			.usage = vk::ImageUsageFlagBits::eSampled |
		             vk::ImageUsageFlagBits::eTransferDst,
		});
	}

	AllocationIndex allocationIndex = createResources(textureDesc, {});

	for (int i = 0; i < textures.size(); i++) {
		int x, y, c;
		uint8_t *data =
			stbi_load(textures.at(i).path.string().c_str(), &x, &y, &c, 0);

		Image &image = m_images.at(
			m_allocationResources.at(allocationIndex).first.at(i).value
		);

		uint32_t imageMemorySize =
			device.getImageMemoryRequirements(image.image).size;
		SubAllocation stagingAllocation =
			m_stagingAllocation.subAllocate({ .size = imageMemorySize });

		vk::Buffer stagingBuffer = device.createBuffer(vk::BufferCreateInfo {
			.size = imageMemorySize,
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		});
		device.bindBufferMemory(
			stagingBuffer,
			m_stagingAllocation.getAllocation().memory,
			stagingAllocation.offset
		);

		CMP_Texture source {
			.dwWidth = static_cast<CMP_DWORD>(x),
			.dwHeight = static_cast<CMP_DWORD>(y),
			.format = CMP_FORMAT_RGBA_8888,
		};
		CMP_Texture destination {
			.dwWidth = static_cast<CMP_DWORD>(x),
			.dwHeight = static_cast<CMP_DWORD>(y),
			.pData = static_cast<CMP_BYTE *>(
						 m_stagingAllocation.getAllocation().address
					 ) +
			         stagingAllocation.offset,
		};

		if (image.format == vk::Format::eBc5SnormBlock) {
			CompressBlockBC5(
				data,
				2,
				data + 8,
				2,
				((uint8_t *)m_stagingAllocation.getAllocation().address) +
					stagingAllocation.offset
			);
		} else if (image.format == vk::Format::eBc7SrgbBlock)
			CompressBlockBC7(
				data,
				4,
				((uint8_t *)m_stagingAllocation.getAllocation().address) +
					stagingAllocation.offset
			);

		stbi_image_free(data);

		copyToImage(
			stagingBuffer,
			image.image,
			{ .imageSubresource = { .aspectMask =
		                                vk::ImageAspectFlagBits::eColor,
									.layerCount = 1,
									},
		      .imageExtent = image.size }
		);

		device.destroyBuffer(stagingBuffer);
	}
	return allocationIndex;
}

ResourceManager::AllocationIndex ResourceManager::createResources(
	std::vector<ImageDescription> imagesDescription,
	std::vector<BufferDescription> buffersDescription
) {
	if (imagesDescription.size() == 0 && buffersDescription.size() == 0) {
		std::cerr << "Tried to allocate with no resources" << std::endl;
		return 0;
	}

	vk::Device &device = Instance::Get().device;

	uint32_t requiredSize = 0;
	uint32_t resourceCount = 0;

	std::unordered_map<uint32_t, ImageHandle> images;
	std::unordered_map<uint32_t, BufferHandle> buffers;
	std::vector<vk::MemoryRequirements> resourcesRequirements;
	resourcesRequirements.reserve(
		imagesDescription.size() + buffersDescription.size()
	);
	AllocationIndex allocIndex = ++m_allocationCount;

	for (auto &description : imagesDescription) {
		vk::Image image = createImage(description);

		vk::MemoryRequirements requirements =
			device.getImageMemoryRequirements(image);

		ImageHandle handle = { ++m_resourceCounter };

		requiredSize += requirements.size;
		if (requirements.alignment > 0)
			requiredSize += requiredSize % requirements.alignment;

		resourcesRequirements.push_back(requirements);
		images[resourceCount++] = handle;

		m_images[handle.value] = {
			.image = image,
			.format = description.format,
			.size = { description.width,
                     description.height,
                     description.depth, },
			.layout = vk::ImageLayout::eUndefined,
		};

		m_allocationResources[allocIndex].first.push_back(handle);
	}

	for (auto &description : buffersDescription) {
		vk::Buffer buffer = device.createBuffer(vk::BufferCreateInfo {
			.size = description.size,
			.usage = description.usage,
		});

		vk::MemoryRequirements requirements =
			device.getBufferMemoryRequirements(buffer);

		requiredSize += requirements.size;
		if (requirements.alignment > 0)
			requiredSize += requiredSize % requirements.alignment;

		resourcesRequirements.push_back(requirements);

		BufferHandle handle = { ++m_resourceCounter };
		buffers[resourceCount++] = handle;
		m_buffers[handle.value] = Buffer {
			.buffer = buffer,
			.size = description.size,
		};
		m_allocationResources[allocIndex].second.push_back(handle);
	}

	m_allocations.try_emplace(
		allocIndex, vk::MemoryPropertyFlagBits::eDeviceLocal, requiredSize
	);

	for (int i = 0; i < resourcesRequirements.size(); i++) {
		if (images.contains(i)) {
			Image &image = m_images.at(images.at(i).value);
			image.allocation = m_allocations.at(allocIndex)
			                       .subAllocate(resourcesRequirements.at(i));

			device.bindImageMemory(
				image.image,
				m_allocations.at(allocIndex).getAllocation().memory,
				image.allocation->offset
			);

			image.view = device.createImageView( {
				.image = image.image,
				.viewType = image.size.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
				.format = image.format,
				.subresourceRange = { .aspectMask =
										Image::GetAspectFlags(image.format),
									.baseMipLevel = 0,
									.levelCount = 1,
									.baseArrayLayer = 0,
									.layerCount = 1,
									},
			});

		}

		else if (buffers.contains(i)) {
			Buffer &buffer = m_buffers.at(buffers.at(i).value);
			buffer.allocation = m_allocations.at(allocIndex)
			                        .subAllocate(resourcesRequirements.at(i));

			device.bindBufferMemory(
				buffer.buffer,
				m_allocations.at(allocIndex).getAllocation().memory,
				buffer.allocation.offset
			);
		}
	}
	return allocIndex;
}
