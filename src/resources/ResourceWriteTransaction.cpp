#include "ResourceWriteTransaction.hpp"

#include "Instance.hpp"

ResourceWriteTransaction::ResourceWriteTransaction(vk::CommandPool &commandPool, ResourceManager &resourceManager) :
	m_resourceManager(resourceManager) {
	m_commandBuffer = Instance::Get().device.allocateCommandBuffers(
		vk::CommandBufferAllocateInfo {
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		}
	)[0];

	m_commandBuffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
}

void ResourceWriteTransaction::copy(const BufferReference &srcInfo, const BufferReference &dstInfo) {
	vk::BufferCopy2 region {
		.srcOffset = srcInfo.offset,
		.dstOffset = dstInfo.offset,
		.size = srcInfo.size,
	};

	m_commandBuffer.copyBuffer2(
		{
			.srcBuffer = m_resourceManager.getBuffer(srcInfo.handle).buffer,
			.dstBuffer = m_resourceManager.getBuffer(dstInfo.handle).buffer,
			.regionCount = 1,
			.pRegions = &region,
		}
	);
}

void ResourceWriteTransaction::copy(const BufferReference &srcInfo, const ImageReference &dstInfo) {
	Buffer &source = m_resourceManager.getBuffer(srcInfo.handle);
	Image &destination = m_resourceManager.getImage(dstInfo.handle);

	vk::Extent3D mipExtent = destination.size;

	mipExtent.width /= 1 << dstInfo.mipLevel;
	mipExtent.height /= 1 << dstInfo.mipLevel;

	vk::BufferImageCopy2 region {
			.bufferOffset = srcInfo.offset,
			.imageSubresource = {
			    .aspectMask = destination.getAspectFlags(),
                  .mipLevel = dstInfo.mipLevel,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
			},
			.imageExtent = mipExtent,
		};

	vk::ImageMemoryBarrier2 preTransferBarrier {
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = dstInfo.initialLayout,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.image = destination.image,
			.subresourceRange = {
          		.aspectMask = destination.getAspectFlags(),
            		.baseMipLevel = dstInfo.mipLevel,
            		.levelCount = 1,
            		.baseArrayLayer = 0,
            		.layerCount = 1,
      		},
		};

	m_commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &preTransferBarrier,
		}
	);

	m_commandBuffer.copyBufferToImage2(
		vk::CopyBufferToImageInfo2 {
			.srcBuffer = source.buffer,
			.dstImage = destination.image,
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &region,
		}
	);

	vk::ImageMemoryBarrier2 postTransferBarrier {
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = dstInfo.finalLayout,
			.image = destination.image,
			.subresourceRange = {
          		.aspectMask = destination.getAspectFlags(),
            		.baseMipLevel = dstInfo.mipLevel,
            		.levelCount = 1,
            		.baseArrayLayer = 0,
            		.layerCount = 1,
      		},
		};

	m_commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &postTransferBarrier,
		}
	);
}

void ResourceWriteTransaction::imageClear(const ImageReference &reference, const vk::ClearColorValue &value) {
	auto &image = m_resourceManager.getImage(reference.handle);

	vk::ImageSubresourceRange subresource = {
		.aspectMask = image.getAspectFlags(),
		.baseMipLevel = reference.mipLevel,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1,
	};
	vk::ImageMemoryBarrier2 preClearBarrier {
		.dstStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = reference.initialLayout,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.image = image.image,
		.subresourceRange = subresource,
	};

	m_commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &preClearBarrier,
		}
	);

	m_commandBuffer.clearColorImage(image.image, reference.initialLayout, &value, 1, &subresource);

	vk::ImageMemoryBarrier2 postClearBarrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = vk::ImageLayout::eTransferDstOptimal,
		.newLayout = reference.finalLayout,
		.image = image.image,
		.subresourceRange = subresource,
	};

	m_commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &postClearBarrier,
		}
	);
}

void ResourceWriteTransaction::submit(vk::Queue &queue, vk::Semaphore &semaphore, uint64_t signalValue) {
	m_commandBuffer.end();

	vk::TimelineSemaphoreSubmitInfo timelineSemaphoreInfo {
		.signalSemaphoreValueCount = 1,
		.pSignalSemaphoreValues = &signalValue,
	};

	vk::SubmitInfo info {
		.pNext = timelineSemaphoreInfo,
		.commandBufferCount = 1,
		.pCommandBuffers = &m_commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &semaphore,
	};

	queue.submit(info);
}
