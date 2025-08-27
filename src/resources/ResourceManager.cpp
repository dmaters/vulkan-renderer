#include "ResourceManager.hpp"

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
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
	m_stagingAllocation(vk::MemoryPropertyFlagBits::eHostVisible, 1 << 28) {
	Instance &instance = Instance::Get();

	m_transferPool = instance.device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = instance.queueFamiliesIndices.transferIndex,
	});
	m_graphicPool = instance.device.createCommandPool({
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = instance.queueFamiliesIndices.graphicsIndex,
	});

	vk::SemaphoreTypeCreateInfo type {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};

	m_semaphore = instance.device.createSemaphore({
		.pNext = &type,
	});

	m_stagingBuffer = instance.device.createBuffer(vk::BufferCreateInfo {
		.size = 1 << 28,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	});
	instance.device.bindBufferMemory(
		m_stagingBuffer, m_stagingAllocation.getAllocation().memory, 0
	);

	m_stagingCommandBuffer =
		instance.device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
			.commandPool = m_transferPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		})[0];

	m_stagingCommandBuffer.begin({
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
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

	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_transferPool,
		.level = vk::CommandBufferLevel::eSecondary,
		.commandBufferCount = 1,

	};
	vk::CommandBuffer commandBuffer =
		instance.device.allocateCommandBuffers(commandBufferInfo)[0];
	vk::CommandBufferInheritanceInfo inheritanceInfo {};
	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = &inheritanceInfo,
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

	m_stagingCommandBuffer.executeCommands(commandBuffer);
};

void copyToImage(
	vk::Buffer origin,
	vk::Image destination,
	vk::BufferImageCopy offset,
	vk::CommandBuffer &commandBuffer
) {
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

void writeMipMaps(
	vk::Image image,
	vk::Extent3D baseResolution,
	uint32_t mipLevels,
	vk::CommandBuffer &commandBuffer
) {
	vk::ImageMemoryBarrier2 sourceBarrier {
		.image = image, .subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.levelCount = 1,
			.layerCount = 1,
		},
	};
	vk::ImageMemoryBarrier2 destinationBarrier = {
		.dstStageMask = vk::PipelineStageFlagBits2::eBlit,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.image = image,
	 	.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.levelCount = 1,
			.layerCount = 1,
		},

	};

	vk::Extent3D res = baseResolution;
	for (uint32_t i = 1; i < mipLevels; i++) {
		destinationBarrier.subresourceRange.baseMipLevel = i;

		sourceBarrier.subresourceRange.baseMipLevel = i - 1;
		sourceBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		sourceBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		sourceBarrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
		sourceBarrier.srcStageMask = vk::PipelineStageFlagBits2::eBlit;
		sourceBarrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
		sourceBarrier.dstStageMask = vk::PipelineStageFlagBits2::eBlit;

		std::array<vk::ImageMemoryBarrier2, 2> barriers = {
			sourceBarrier,
			destinationBarrier,
		};
		commandBuffer.pipelineBarrier2(vk::DependencyInfo {
			.imageMemoryBarrierCount = 2,
			.pImageMemoryBarriers = barriers.data(),
		});

		vk::ImageBlit blit {
			.srcSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i - 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
			},
			.dstSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = i,
                .baseArrayLayer = 0,
                .layerCount = 1,
			},
		};
		blit.srcOffsets[0] = vk::Offset3D { 0, 0, 0 };
		blit.srcOffsets[1] =
			vk::Offset3D { (int32_t)res.width, (int32_t)res.height, 1 };
		blit.dstOffsets[0] = vk::Offset3D { 0, 0, 0 };
		blit.dstOffsets[1] =
			vk::Offset3D { (int32_t)res.width / 2, (int32_t)res.height / 2, 1 };

		commandBuffer.blitImage(
			image,
			vk::ImageLayout::eTransferSrcOptimal,
			image,
			vk::ImageLayout::eTransferDstOptimal,
			1,
			&blit,
			vk::Filter::eLinear
		);

		sourceBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		sourceBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		commandBuffer.pipelineBarrier2(vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &sourceBarrier,
		});

		res.width /= 2;
		res.height /= 2;
	}
}
enum ChannelsValues : uint8_t {
	R = 1 << 0,
	G = 1 << 1,
	B = 1 << 2,
	A = 1 << 3,
};
typedef uint8_t Channels;

void loadImage(
	std::filesystem::path &path, std::byte *stagingAddress, Channels channels
) {
	int x, y, c;

	std::byte *data =
		(std::byte *)stbi_load(path.string().c_str(), &x, &y, &c, 4);

	if (!data) std::cerr << stbi_failure_reason() << std::endl;
	uint8_t channelCount = 0;
	if (channels & ChannelsValues::R) channelCount++;
	if (channels & ChannelsValues::G) channelCount++;
	if (channels & ChannelsValues::B) channelCount++;
	if (channels & ChannelsValues::A) channelCount++;

	for (int i = 0; i < x * y; i++) {
		uint8_t texelOffset = 0;
		if (channels & ChannelsValues::R) {
			stagingAddress[i * channelCount] = data[i * 4];
			texelOffset++;
		}
		if (channels & ChannelsValues::G) {
			stagingAddress[i * channelCount + texelOffset] = data[i * 4 + 1];
			texelOffset++;
		}
		if (channels & ChannelsValues::B) {
			stagingAddress[i * channelCount + texelOffset] = data[i * 4 + 2];
			texelOffset++;
		}
		if (channels & ChannelsValues::A) {
			stagingAddress[i * channelCount + texelOffset] = data[i * 4 + 3];
			texelOffset++;
		}
	}

	stbi_image_free(data);
}

ResourceManager::AllocationIndex ResourceManager::loadSceneTextures(
	std::vector<TextureInfo> textures
) {
	stbi_set_flip_vertically_on_load(true);
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
			.miplevels = (uint32_t)(std::floor(std::log2(std::max(x, y))) + 1),
			.format = info.expectedFormat,
			.usage = vk::ImageUsageFlagBits::eSampled |
		             vk::ImageUsageFlagBits::eTransferSrc |
		             vk::ImageUsageFlagBits::eTransferDst,
		});
	}

	AllocationIndex allocationIndex = createResources(textureDesc, {});

	uint32_t size = m_allocations.at(allocationIndex).getAllocation().size;

	LinearAllocator stagingAllocator(
		vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent,
		size
	);

	vk::Buffer stagingBuffer = device.createBuffer({
		.size = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	});
	device.bindBufferMemory(
		stagingBuffer, stagingAllocator.getAllocation().memory, 0
	);

	vk::CommandBuffer transferBuffer = device.allocateCommandBuffers({
		.commandPool = m_transferPool,
		.level = vk::CommandBufferLevel::eSecondary,
		.commandBufferCount = 1,

	})[0];
	vk::CommandBuffer mipmapBuffer = device.allocateCommandBuffers({
		.commandPool = m_graphicPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	})[0];
	vk::CommandBufferInheritanceInfo inheritanceInfo {};
	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = &inheritanceInfo,
	};

	transferBuffer.begin(beginInfo);
	mipmapBuffer.begin(beginInfo);

	std::vector<std::thread> threads;

	for (int i = 0; i < textures.size(); i++) {
		Image &image = m_images.at(
			m_allocationResources.at(allocationIndex).first.at(i).value
		);

		vk::MemoryRequirements requirements =
			device.getImageMemoryRequirements(image.image);
		Channels expectedChannels =
			image.format == vk::Format::eR8G8Unorm
				? (ChannelsValues::R | ChannelsValues::G)
				: (0xff);

		SubAllocation stagingAllocation = stagingAllocator.subAllocate({
			.size = requirements.size,
			.alignment = requirements.alignment,
		});

		std::byte *address =
			stagingAllocator.getAllocation().address + stagingAllocation.offset;
		threads.emplace_back(
			[&path = textures[i].path, address, expectedChannels]() {
				loadImage(path, address, expectedChannels);
			}
		);

		copyToImage(
			stagingBuffer,
			image.image,
			{
				.bufferOffset = stagingAllocation.offset,
				.bufferRowLength  = (uint32_t)image.size.width,
				.imageSubresource = { .aspectMask =
		                                vk::ImageAspectFlagBits::eColor,
									.layerCount = 1,
									},
		      .imageExtent = image.size,
			 },
			 transferBuffer
		);

		writeMipMaps(
			image.image, image.size, textureDesc[i].miplevels, mipmapBuffer
		);
	}
	transferBuffer.end();
	m_stagingCommandBuffer.executeCommands(transferBuffer);

	mipmapBuffer.end();
	for (auto &thread : threads) thread.join();
	uint64_t waitValue = m_transferCount + 1;
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTransfer;
	vk::TimelineSemaphoreSubmitInfo waitSemaphoreInfo {
		.waitSemaphoreValueCount = 1,
		.pWaitSemaphoreValues = &waitValue,
	};
	vk::SubmitInfo mipmapSubmitInfo {
		.pNext = &waitSemaphoreInfo,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_semaphore,
		.pWaitDstStageMask = &waitStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &mipmapBuffer,
	};

	Instance::Get().graphicQueue.submit({ mipmapSubmitInfo });

	m_stagingAdditionalAllocators.push_back(stagingAllocator);

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

	device.waitSemaphores(
		vk::SemaphoreWaitInfo {
			.semaphoreCount = 1,
			.pSemaphores = &m_semaphore,
			.pValues = &m_transferCount,
		},
		UINT64_MAX
	);

	uint32_t requiredSize = 0;
	uint32_t resourceCount = 0;

	std::unordered_map<uint32_t, ImageHandle> images;
	std::unordered_map<uint32_t, BufferHandle> buffers;
	std::vector<vk::MemoryRequirements> resourcesRequirements;

	uint32_t allocationSize =
		imagesDescription.size() + buffersDescription.size();
	resourcesRequirements.reserve(allocationSize);

	AllocationIndex allocIndex = ++m_allocationCount;

	for (auto &description : imagesDescription) {
		vk::Image image = createImage(description);

		vk::MemoryRequirements requirements =
			device.getImageMemoryRequirements(image);

		ImageHandle handle = { ++m_resourceCounter };

		requiredSize += requirements.size + requirements.alignment;

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

		requiredSize += requirements.size + requirements.alignment;

		resourcesRequirements.push_back(requirements);

		BufferHandle handle = { ++m_resourceCounter };
		buffers[resourceCount++] = handle;
		m_buffers[handle.value] = Buffer {
			.buffer = buffer,
			.size = description.size,
		};
		m_allocationResources[allocIndex].second.push_back(handle);
	}

	m_allocations.emplace(
		allocIndex,
		LinearAllocator(vk::MemoryPropertyFlagBits::eDeviceLocal, requiredSize)
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
									.levelCount = imagesDescription[i].miplevels,
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

void ResourceManager::freeAllocation(AllocationIndex index) {
	vk::Device &device = Instance::Get().device;
	for (auto &image : m_allocationResources[index].first) {
		device.destroyImage(m_images[image.value].image);
		m_images.erase(image.value);
	}

	for (auto &buffer : m_allocationResources[index].second) {
		device.destroyBuffer(m_buffers[buffer.value].buffer);
		m_buffers.erase(buffer.value);
	}

	m_allocationResources.erase(index);
	m_allocations.at(index).getAllocation().free();
	m_allocations.erase(index);
}

uint64_t ResourceManager::sync() {
	vk::Device &device = Instance::Get().device;

	m_stagingCommandBuffer.end();

	vk::TimelineSemaphoreSubmitInfo semaphoreInfo {
		.signalSemaphoreValueCount = 1,
		.pSignalSemaphoreValues = &(++m_transferCount),
	};
	vk::SubmitInfo submit {
		.pNext = &semaphoreInfo,
		.commandBufferCount = 1,
		.pCommandBuffers = &m_stagingCommandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_semaphore,
	};

	Instance::Get().transferQueue.submit({ submit });

	std::thread([allocations = m_stagingAdditionalAllocators,
	             &device,
	             waitValue = m_transferCount,
	             &semaphore = m_semaphore]() mutable {
		device.waitSemaphores(
			vk::SemaphoreWaitInfo {
				.semaphoreCount = 1,
				.pSemaphores = &semaphore,
				.pValues = &waitValue,
			},
			UINT64_MAX
		);
		for (auto &allocator : allocations) allocator.getAllocation().free();
	}).detach();

	m_stagingAdditionalAllocators.clear();

	m_stagingCommandBuffer = device.allocateCommandBuffers({
		.commandPool = m_transferPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	})[0];

	m_stagingCommandBuffer.begin({
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	});

	return m_transferCount;
}