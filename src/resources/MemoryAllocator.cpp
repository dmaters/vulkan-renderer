#include "MemoryAllocator.hpp"

#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Image.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

MemoryAllocator::MemoryAllocator(Instance& instance) : m_instance(instance) {
	const auto& d = VULKAN_HPP_DEFAULT_DISPATCHER;
	VmaVulkanFunctions functions {
		.vkGetInstanceProcAddr = d.vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = d.vkGetDeviceProcAddr,
	};

	VmaAllocatorCreateInfo allocatorInfo {
		.flags = {},
		.physicalDevice = instance.physicalDevice,
		.device = instance.device,
		.preferredLargeHeapBlockSize = {},
		.pAllocationCallbacks = {},
		.pDeviceMemoryCallbacks = {},
		.pHeapSizeLimit = {},
		.pVulkanFunctions = &functions,
		.instance = instance.instance,
		.vulkanApiVersion = vk::ApiVersion13,
		.pTypeExternalMemoryHandleTypes = {},

	};

	vmaCreateAllocator(&allocatorInfo, &m_allocator);

	m_queue = instance.device.getQueue(
		instance.queueFamiliesIndices.transferIndex, 0
	);
	vk::CommandPoolCreateInfo commandPoolInfo {
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = instance.queueFamiliesIndices.transferIndex
	};

	m_commandPool = instance.device.createCommandPool(commandPoolInfo);

	m_uploadFinishedFence = m_instance.device.createFence({});
}

constexpr VmaAllocationCreateInfo AllocationInfo(
	MemoryAllocator::Location location
);
MemoryAllocator::BufferAllocation MemoryAllocator::allocateBuffer(
	const vk::BufferCreateInfo& bufferInfo, Location location
) {
	const VmaAllocationCreateInfo createInfo = AllocationInfo(location);

	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;

	vmaCreateBuffer(
		m_allocator,
		&(const VkBufferCreateInfo&)bufferInfo,
		&createInfo,
		&buffer,
		&allocation,
		&info
	);

	return BufferAllocation {
		.allocation = allocation,
		.buffer = buffer,
		.mappedData = location == Location::HostMapped ? info.pMappedData
		                                               : nullptr,
	};
}
MemoryAllocator::BufferAllocation MemoryAllocator::allocateStagingBuffer(
	size_t size
) {
	vk::BufferCreateInfo info {
		.size = size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	};

	return allocateBuffer(info, Location::HostMapped);
}

MemoryAllocator::ImageAllocation MemoryAllocator::allocateImage(
	const vk::ImageCreateInfo& imageInfo
) {
	const VmaAllocationCreateInfo createInfo =
		AllocationInfo(Location::DeviceLocal);

	VkImage image;
	VmaAllocation allocation;
	VmaAllocationInfo info;

	m_instance.physicalDevice.getImageFormatProperties(
		imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage
	);

	vmaCreateImage(
		m_allocator,
		(const VkImageCreateInfo*)&imageInfo,
		&createInfo,
		&image,
		&allocation,
		&info
	);

	return ImageAllocation {
		.allocation = allocation,
		.image = image,
	};
}
// TODO: Change approach
void MemoryAllocator::copyBuffer(
	BufferAllocation& origin,
	BufferAllocation& destination,
	vk::BufferCopy offset
) {
	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};
	vk::CommandBuffer commandBuffer =
		m_instance.device.allocateCommandBuffers(commandBufferInfo)[0];

	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};
	commandBuffer.begin(beginInfo);

	commandBuffer.copyBuffer(origin.buffer, destination.buffer, offset);
	commandBuffer.end();
	vk::SubmitInfo submitInfo { .commandBufferCount = 1,
		                        .pCommandBuffers = &commandBuffer };

	m_queue.submit({ submitInfo }, m_uploadFinishedFence);
	m_instance.device.waitForFences({ m_uploadFinishedFence }, true, 5e6);
	m_instance.device.resetFences({ m_uploadFinishedFence });
};
void MemoryAllocator::copyToImage(
	BufferAllocation& origin,
	ImageAllocation& destination,
	vk::BufferImageCopy offset
) {
	vk::CommandBufferAllocateInfo commandBufferInfo {
		.commandPool = m_commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,

	};

	vk::CommandBuffer commandBuffer =
		m_instance.device.allocateCommandBuffers(commandBufferInfo)[0];

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

	m_queue.submit({ submitInfo }, m_uploadFinishedFence);
	m_instance.device.waitForFences({ m_uploadFinishedFence }, true, 5e6);
	m_instance.device.resetFences({ m_uploadFinishedFence });
}
constexpr VmaAllocationCreateInfo AllocationInfo(
	MemoryAllocator::Location location
) {
	switch (location) {
		case MemoryAllocator::Location::DeviceLocal:
			return VmaAllocationCreateInfo {
				.flags = {},
				.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
				.requiredFlags = VkMemoryPropertyFlagBits::
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				.preferredFlags = VkMemoryPropertyFlagBits::
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				.memoryTypeBits = UINT32_MAX,
				.pool = nullptr,
				.pUserData = nullptr,
				.priority = 1
			};
		case MemoryAllocator::Location::HostMapped:
			return VmaAllocationCreateInfo {
				.flags =
					VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
					VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
				.requiredFlags = VkMemoryPropertyFlagBits::
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				.preferredFlags = VkMemoryPropertyFlagBits::
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				.memoryTypeBits = UINT32_MAX,
				.pool = nullptr,
				.pUserData = nullptr,
				.priority = 1
			};
			break;
	}
}

void MemoryAllocator::copyToBuffer(
	const std::vector<std::byte>& data, BufferAllocation& buffer
) {
	VmaAllocationInfo info;
	vmaGetAllocationInfo(m_allocator, buffer.allocation, &info);

	if (info.pMappedData == nullptr) {
		MemoryAllocator::BufferAllocation staging =
			allocateStagingBuffer(data.size());
		VmaAllocationInfo stagingInfo;
		vmaGetAllocationInfo(m_allocator, staging.allocation, &stagingInfo);
		std::memcpy((char*)stagingInfo.pMappedData, data.data(), data.size());

		copyBuffer(staging, buffer, { .size = data.size() });
		freeAllocation(staging.allocation);
	} else {
		memcpy((char*)info.pMappedData, data.data(), data.size());
	}
}

void MemoryAllocator::freeAllocation(VmaAllocation& allocation) {
	vmaFreeMemory(m_allocator, allocation);
}