#include "ResourceManager.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <texture_compressor/compression.hpp>
#include <thread>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

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
	m_stagingAllocation(
		vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCached,
		1 << 28
	) {
	Instance &instance = Instance::Get();

	m_transferPool = instance.device.createCommandPool(
		{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = instance.queueFamiliesIndices.transferIndex,
		}
	);
	m_graphicPool = instance.device.createCommandPool(
		{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = instance.queueFamiliesIndices.graphicsIndex,
		}
	);

	vk::SemaphoreTypeCreateInfo type {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};

	m_semaphore = instance.device.createSemaphore(
		{
			.pNext = &type,
		}
	);

	m_stagingBuffer = instance.device.createBuffer(
		vk::BufferCreateInfo {
			.size = 1 << 28,
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		}
	);
	instance.device.bindBufferMemory(
		m_stagingBuffer, m_stagingAllocation.getAllocation().memory, 0
	);

	m_stagingCommandBuffer = instance.device.allocateCommandBuffers(
		vk::CommandBufferAllocateInfo {
			.commandPool = m_transferPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		}
	)[0];

	m_stagingCommandBuffer.begin(
		{
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		}
	);
}

void ResourceManager::setName(std::string name, ImageHandle handle) {
	Instance::Get().device.setDebugUtilsObjectNameEXT(
		vk::DebugUtilsObjectNameInfoEXT {
			.objectType = vk::ObjectType::eImage,
			.objectHandle = (uint64_t)(VkImage)m_images.at(handle.value).image,
			.pObjectName = name.data(),
		}
	);
	m_imageNames[name] = handle;
}
void ResourceManager::setName(std::string name, BufferHandle handle) {
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

		barriers.push_back(
			{
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
				.dstAccessMask = vk::AccessFlagBits2::eNone,
				.srcQueueFamilyIndex =
					instance.queueFamiliesIndices.transferIndex,
				.dstQueueFamilyIndex =
					instance.queueFamiliesIndices.graphicsIndex,
				.buffer = copyInfo.destination,
				.offset = 0,
				.size = copyInfo.copy.size,
			}
		);
	}

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount = (uint32_t)barriers.size(),
			.pBufferMemoryBarriers = barriers.data(),
		}
	);

	commandBuffer.end();

	m_stagingCommandBuffer.executeCommands(commandBuffer);
};

void copyToImage(
	vk::Buffer origin,
	vk::Image destination,
	std::vector<vk::BufferImageCopy> offsets,
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
            .levelCount = (uint32_t)offsets.size(),
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
	commandBuffer.pipelineBarrier2(
		{
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		}
	);
	commandBuffer.copyBufferToImage(
		origin,
		destination,
		vk::ImageLayout::eTransferDstOptimal,
		{ offsets }
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

enum ChannelsValues : uint8_t {
	R = 1 << 0,
	G = 1 << 1,
	B = 1 << 2,
	A = 1 << 3,
};
typedef uint8_t Channels;

void loadImage(
	std::filesystem::path &path,
	std::byte *stagingAddress,
	ResourceManager::TextureInfo::TextureType textureType,
	uint32_t mipLevels
) {
	Channels channels = 0;
	switch (textureType) {
		case ResourceManager::TextureInfo::TextureType::Albedo:
			channels = (ChannelsValues::R) | (ChannelsValues::G) |
			           (ChannelsValues::B) | (ChannelsValues::A);
			break;
		case ResourceManager::TextureInfo::TextureType::Normal:
			channels = (ChannelsValues::R) | (ChannelsValues::G);
			break;
		case ResourceManager::TextureInfo::TextureType::MetallicRoughness:
			channels = (ChannelsValues::G) | (ChannelsValues::B);
			break;
		default:
			channels = (ChannelsValues::R) | (ChannelsValues::G) |
			           (ChannelsValues::B) | (ChannelsValues::A);
			break;
	}

	int x, y, c;

	std::byte *data =
		(std::byte *)stbi_load(path.string().c_str(), &x, &y, &c, 4);

	if (!data) std::cerr << stbi_failure_reason() << std::endl;
	uint8_t channelCount = 0;
	if (channels & ChannelsValues::R) channelCount++;
	if (channels & ChannelsValues::G) channelCount++;
	if (channels & ChannelsValues::B) channelCount++;
	if (channels & ChannelsValues::A) channelCount++;

	std::vector<std::byte> uncompressedData(x * y * channelCount);

	for (int i = 0; i < x * y; i++) {
		uint8_t texelOffset = 0;
		if (channels & ChannelsValues::R) {
			uncompressedData[i * channelCount] = data[i * 4];
			texelOffset++;
		}
		if (channels & ChannelsValues::G) {
			uncompressedData[i * channelCount + texelOffset] = data[i * 4 + 1];
			texelOffset++;
		}
		if (channels & ChannelsValues::B) {
			uncompressedData[i * channelCount + texelOffset] = data[i * 4 + 2];
			texelOffset++;
		}
		if (channels & ChannelsValues::A) {
			uncompressedData[i * channelCount + texelOffset] = data[i * 4 + 3];
			texelOffset++;
		}
	}

	texture_compressor::Format format;
	if (textureType == ResourceManager::TextureInfo::TextureType::Normal ||
	    textureType ==
	        ResourceManager::TextureInfo::TextureType::MetallicRoughness)
		format = texture_compressor::Format::BC5;
	else
		format = texture_compressor::Format::BC1_ALPHA;

	texture_compressor::compress(
		x, y, format, uncompressedData.data(), stagingAddress, mipLevels
	);

	stbi_image_free(data);
}

ResourceManager::DeviceAllocationIndex ResourceManager::loadSceneTextures(
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

		uint32_t mipLevels = std::floor(std::log2(std::min(x,y))) + 1;
		mipLevels = mipLevels < 3 ? 1 : mipLevels - 2;

		textureDesc.push_back(
			{
				.width = (uint32_t)x,
				.height = (uint32_t)y,
				.depth = 1,
				.miplevels = mipLevels,
				.format = info.getFormat(),
				.usage = vk::ImageUsageFlagBits::eSampled |
		                 vk::ImageUsageFlagBits::eTransferSrc |
		                 vk::ImageUsageFlagBits::eTransferDst,
			}
		);
	}

	DeviceAllocationIndex allocationIndex = createResources(textureDesc, {});

	uint32_t size =
		m_deviceAllocations.at(allocationIndex).getAllocation().size;

	LinearAllocator stagingAllocator(
		vk::MemoryPropertyFlagBits::eHostVisible |
			vk::MemoryPropertyFlagBits::eHostCoherent,
		size
	);

	vk::Buffer stagingBuffer = device.createBuffer(
		{
			.size = size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		}
	);
	device.bindBufferMemory(
		stagingBuffer, stagingAllocator.getAllocation().memory, 0
	);

	vk::CommandBuffer transferBuffer = device.allocateCommandBuffers(
		{
			.commandPool = m_transferPool,
			.level = vk::CommandBufferLevel::eSecondary,
			.commandBufferCount = 1,

		}
	)[0];

	vk::CommandBufferInheritanceInfo inheritanceInfo {};
	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		.pInheritanceInfo = &inheritanceInfo,
	};

	transferBuffer.begin(beginInfo);

	std::vector<std::thread> threads;

	for (int i = 0; i < textures.size(); i++) {

		Image &image =
			m_images[m_deviceAllocatedImages[allocationIndex][i].value];

		vk::MemoryRequirements requirements =
			device.getImageMemoryRequirements(image.image);

		SubAllocation stagingAllocation = stagingAllocator.subAllocate(
			{
				.size = requirements.size,
				.alignment = requirements.alignment,
			}
		);

		uint32_t mipLevels = std::floor(std::log2(std::min(image.size.width, image.size.height))) + 1;
		mipLevels = mipLevels < 3 ? 1 : mipLevels - 2;

		std::byte *address =
			stagingAllocator.getAllocation().address + stagingAllocation.offset;
		threads.emplace_back([&textures, i, address, mipLevels]() {
			loadImage(textures[i].path, address, textures[i].textureType, mipLevels);
		});

		std::vector<vk::BufferImageCopy> copyInfos;
		uint32_t baseOffset = 0;

		for(uint32_t m = 0; m < mipLevels; m++){

		    uint32_t mipWidth  = std::max(1u, image.size.width  >> m);
			uint32_t mipHeight = std::max(1u, image.size.height >> m);

            uint32_t blockWidth = (mipWidth  + 3) / 4;
            uint32_t blockHeight = (mipHeight + 3) / 4;

            uint8_t blockSize = 0;
           	if(textures[i].textureType == TextureInfo::TextureType::Albedo)
                blockSize = 8;
            else
                blockSize = 16;

            uint32_t mipSize = blockWidth * blockHeight * blockSize;

		    copyInfos.push_back({
    			.bufferOffset = stagingAllocation.offset + baseOffset,
    			.bufferRowLength  = 0,
    			.imageSubresource = {
          	        .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = m,
    				.layerCount = 1,
                },
    	        .imageExtent = {
					.width = mipWidth,
					.height = mipHeight,
					.depth = 1
				},
    		 });

			baseOffset += mipSize;
		}
		copyToImage(
			stagingBuffer,
			image.image,
			copyInfos,
		    transferBuffer
		);
	}
	transferBuffer.end();

	for (auto &thread : threads) thread.join();

	m_stagingCommandBuffer.executeCommands(transferBuffer);
	sync();

	m_stagingAdditionalAllocators.push_back(stagingAllocator);

	return allocationIndex;
}

ResourceManager::DeviceAllocationIndex ResourceManager::createResources(
	std::vector<ImageDescription> imagesDescriptions,
	std::vector<BufferDescription> buffersDescriptions
) {
	if (imagesDescriptions.empty() && buffersDescriptions.empty()) {
		std::cerr << "Tried to allocate with no resources" << std::endl;
		return 0;
	}

	assert(
		std::find_if(
			buffersDescriptions.begin(),
			buffersDescriptions.end(),
			[](BufferDescription &desc) { return desc.size == 0; }
		) == buffersDescriptions.end()
	);

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
		imagesDescriptions.size() + buffersDescriptions.size();
	resourcesRequirements.reserve(allocationSize);

	DeviceAllocationIndex allocIndex = ++m_allocationCount;

	for (int i = 0; i < imagesDescriptions.size(); i++) {
		auto &description = imagesDescriptions[i];

		vk::Image image = createImage(description);

		vk::MemoryRequirements requirements =
			device.getImageMemoryRequirements(image);

		ImageHandle handle = { ++m_resourceCounter };

		requiredSize = (requiredSize + requirements.alignment - 1) &
		               ~(requirements.alignment - 1);
		requiredSize += requirements.size;

		resourcesRequirements.push_back(requirements);
		images[resourceCount++] = handle;

		m_images[handle.value] = {
			.image = image,
			.format = description.format,
			.size = { description.width,
                     description.height,
                     description.depth,
					},
		};

		m_deviceAllocatedImages[allocIndex].push_back(handle);
	}

	for (int i = 0; i < buffersDescriptions.size(); i++) {
		auto &description = buffersDescriptions[i];

		vk::Buffer buffer = device.createBuffer(
			vk::BufferCreateInfo {
				.size = description.size,
				.usage = description.usage,
			}
		);

		vk::MemoryRequirements requirements =
			device.getBufferMemoryRequirements(buffer);

		requiredSize = (requiredSize + requirements.alignment - 1) &
		               ~(requirements.alignment - 1);
		requiredSize += requirements.size;

		resourcesRequirements.push_back(requirements);

		BufferHandle handle = { ++m_resourceCounter };

		buffers[resourceCount++] = handle;
		m_buffers[handle.value] = Buffer {
			.buffer = buffer,
			.size = description.size,
		};
		m_deviceAllocatedBuffers[allocIndex].push_back(handle);
	}

	m_deviceAllocations.emplace(
		allocIndex,
		LinearAllocator(vk::MemoryPropertyFlagBits::eDeviceLocal, requiredSize)
	);

	for (int i = 0; i < resourcesRequirements.size(); i++) {
		if (images.contains(i)) {
			Image &image = m_images.at(images.at(i).value);
			image.allocation = m_deviceAllocations.at(allocIndex)
			                       .subAllocate(resourcesRequirements.at(i));

			device.bindImageMemory(
				image.image,
				m_deviceAllocations.at(allocIndex).getAllocation().memory,
				image.allocation->offset
			);

			image.view = device.createImageView( {
				.image = image.image,
				.viewType = image.size.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
				.format = image.format,
				.subresourceRange = { .aspectMask =
										Image::GetAspectFlags(image.format),
									.baseMipLevel = 0,
									.levelCount = imagesDescriptions[i].miplevels,
									.baseArrayLayer = 0,
									.layerCount = 1,
									},
			});

		}

		else if (buffers.contains(i)) {
			Buffer &buffer = m_buffers.at(buffers.at(i).value);
			buffer.allocation = m_deviceAllocations.at(allocIndex)
			                        .subAllocate(resourcesRequirements.at(i));

			device.bindBufferMemory(
				buffer.buffer,
				m_deviceAllocations.at(allocIndex).getAllocation().memory,
				buffer.allocation.offset
			);
		}
	}
	return allocIndex;
}
ResourceManager::DeviceAllocationIndex ResourceManager::createSharedAllocation(
	std::vector<BufferDescription> bufferDescriptions
) {
	assert(bufferDescriptions.size() > 0);

	assert(
		std::find_if(
			bufferDescriptions.begin(),
			bufferDescriptions.end(),
			[](BufferDescription &desc) { return desc.size == 0; }
		) == bufferDescriptions.end()
	);

	vk::Device &device = Instance::Get().device;

	uint32_t requiredSize = 0;
	uint32_t resourceCount = 0;

	std::unordered_map<uint32_t, BufferHandle> buffers;
	std::vector<vk::MemoryRequirements> resourcesRequirements;

	uint32_t allocationSize = bufferDescriptions.size();
	resourcesRequirements.reserve(allocationSize);

	DeviceAllocationIndex allocIndex = ++m_allocationCount;

	for (int i = 0; i < bufferDescriptions.size(); i++) {
		auto &description = bufferDescriptions[i];

		vk::Buffer buffer = device.createBuffer(
			vk::BufferCreateInfo {
				.size = description.size,
				.usage = description.usage,
			}
		);

		vk::MemoryRequirements requirements =
			device.getBufferMemoryRequirements(buffer);

		requiredSize = (requiredSize + requirements.alignment - 1) &
		               ~(requirements.alignment - 1);
		requiredSize += requirements.size;

		resourcesRequirements.push_back(requirements);

		BufferHandle handle = { ++m_resourceCounter };

		buffers[resourceCount++] = handle;
		m_buffers[handle.value] = Buffer {
			.buffer = buffer,
			.size = description.size,
		};
		m_deviceAllocatedBuffers[allocIndex].push_back(handle);
	}
	LinearAllocator allocator = LinearAllocator(
		vk::MemoryPropertyFlagBits::eDeviceLocal |
			vk::MemoryPropertyFlagBits::eHostVisible,
		requiredSize
	);
	m_deviceAllocations.emplace(allocIndex, allocator);

	void *address = allocator.getAllocation().address;

	for (int i = 0; i < resourcesRequirements.size(); i++) {
		Buffer &buffer = m_buffers.at(buffers.at(i).value);
		buffer.allocation = m_deviceAllocations.at(allocIndex)
		                        .subAllocate(resourcesRequirements.at(i));
		buffer.data =
			static_cast<std::byte *>(address) + buffer.allocation.offset;

		device.bindBufferMemory(
			buffer.buffer,
			m_deviceAllocations.at(allocIndex).getAllocation().memory,
			buffer.allocation.offset
		);
	}
	return allocIndex;
}
ResourceManager::HostAllocationIndex ResourceManager::createHostAllocation(
	std::vector<uint32_t> bufferSizes
) {
	assert(bufferSizes.size() > 0);
	vk::Device &device = Instance::Get().device;
	HostAllocationIndex allocIndex = ++m_allocationCount;

	uint32_t totalSize = 0;
	for (uint32_t size : bufferSizes) totalSize += size;

	vk::Buffer buffer = device.createBuffer(
		vk::BufferCreateInfo {
			.size = totalSize,
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		}
	);

	uint32_t requiredSize = device.getBufferMemoryRequirements(buffer).size;

	Allocation allocation = Allocation(
		vk::MemoryPropertyFlagBits::eHostCoherent |
			vk::MemoryPropertyFlagBits::eHostCached,
		requiredSize
	);
	m_hostAllocations.emplace(allocIndex, allocation);
	device.bindBufferMemory(buffer, allocation.memory, 0);

	uint32_t offset = 0;

	for (uint32_t size : bufferSizes) {
		BufferHandle bufferHandle = { ++m_resourceCounter };

		m_buffers[bufferHandle.value] = {
			.buffer = buffer,
			.allocation = { .offset = offset, .size = size },
			.size = size,
		};

		m_hostAllocationBuffers[allocIndex].push_back(bufferHandle);

		offset += size;
	}

	void *address = allocation.address;

	for (BufferHandle hostBuffer : m_hostAllocationBuffers[allocIndex]) {
		Buffer &buffer = m_buffers[hostBuffer.value];
		buffer.data =
			static_cast<std::byte *>(address) + buffer.allocation.offset;
	}

	return allocIndex;
}

void ResourceManager::freeDeviceAllocation(
	ResourceManager::DeviceAllocationIndex index
) {
	vk::Device &device = Instance::Get().device;

	for (auto &image : m_deviceAllocatedImages[index]) {
		device.destroyImageView(m_images[image.value].view);
		device.destroyImage(m_images[image.value].image);
		m_images.erase(image.value);
	}

	for (auto &buffer : m_deviceAllocatedBuffers[index]) {
		device.destroyBuffer(m_buffers[buffer.value].buffer);
		m_buffers.erase(buffer.value);
	}
	m_deviceAllocatedBuffers.erase(index);
	m_deviceAllocatedImages.erase(index);

	m_deviceAllocations.at(index).getAllocation().free();
	m_deviceAllocations.erase(index);
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

	m_stagingCommandBuffer = device.allocateCommandBuffers(
		{
			.commandPool = m_transferPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		}
	)[0];

	m_stagingCommandBuffer.begin(
		{
			.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		}
	);

	return m_transferCount;
}
