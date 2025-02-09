#include "MemoryAllocator.hpp"

#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "Image.hpp"
#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

MemoryAllocator::MemoryAllocator(Instance& instance) : m_instance(instance) {
	VmaAllocatorCreateInfo allocatorInfo {
		.flags = {},
		.physicalDevice = instance.physicalDevice,
		.device = instance.device,
		.preferredLargeHeapBlockSize = {},
		.pAllocationCallbacks = {},
		.pDeviceMemoryCallbacks = {},
		.pHeapSizeLimit = {},
		.pVulkanFunctions = {},
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
		.base { .allocation = allocation, .allocator = *this },
		.buffer = buffer,
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
		.base = { .allocation = allocation, .allocator = *this },
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
	Image::LayoutTransition(
		commandBuffer,
		destination.image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal
	);
	commandBuffer.copyBufferToImage(
		origin.buffer,
		destination.image,
		vk::ImageLayout::eTransferDstOptimal,
		{ offset }
	);
	Image::LayoutTransition(
		commandBuffer,
		destination.image,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);
	commandBuffer.end();
	vk::SubmitInfo submitInfo { .commandBufferCount = 1,
		                        .pCommandBuffers = &commandBuffer };

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

void MemoryAllocator::BufferAllocation::updateData(
	const std::vector<unsigned char>& data, unsigned int offset
) {
	VmaAllocationInfo info;
	vmaGetAllocationInfo(base.allocator.m_allocator, base.allocation, &info);

	if (info.pMappedData == nullptr) {
		MemoryAllocator::BufferAllocation staging =
			base.allocator.allocateStagingBuffer(data.size());
		VmaAllocationInfo stagingInfo;
		vmaGetAllocationInfo(
			base.allocator.m_allocator, staging.base.allocation, &stagingInfo
		);
		std::memcpy(
			(char*)stagingInfo.pMappedData + offset, data.data(), data.size()
		);

		base.allocator.copyBuffer(staging, *this, { .size = data.size() });
	} else {
		memcpy((char*)info.pMappedData + offset, data.data(), data.size());
	}
}

void MemoryAllocator::freeAllocation(MemoryAllocator::Allocation& allocation) {
	vmaFreeMemory(m_allocator, allocation.allocation);
}