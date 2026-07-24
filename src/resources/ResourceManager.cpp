#include "ResourceManager.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Buffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "memory/Allocation.hpp"
#include "memory/LinearAllocator.hpp"

ResourceManager::ResourceManager() {
	auto &device = Instance::Get().device;
	m_commandPool = device.createCommandPool(
		{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = Instance::Get().queueFamiliesIndices.transferIndex,
		}
	);
	vk::SemaphoreTypeCreateInfo info {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};
	m_semaphore = device.createSemaphore(
		{
			.pNext = &info,
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
}
void ResourceManager::setName(std::string name, BufferHandle handle) {
	Instance::Get().device.setDebugUtilsObjectNameEXT(
		vk::DebugUtilsObjectNameInfoEXT {
			.objectType = vk::ObjectType::eBuffer,
			.objectHandle = (uint64_t)(VkBuffer)m_buffers.at(handle.value).buffer,
			.pObjectName = name.data(),
		}
	);
}

ImageHandle ResourceManager::registerImage(Image image, ImageHandle handle) {
	if (handle.value == 0) {
		handle = static_cast<ImageHandle>(m_images.size());
	}

	m_images[handle.value] = image;
	return handle;
}

vk::Image createImage(const ResourceManager::ImageDescription &description) {
	vk::Device &device = Instance::Get().device;

	vk::ImageCreateInfo imageInfo {
		.flags = {},
		.imageType = description.depth > 1 ? vk::ImageType::e3D : vk::ImageType::e2D,
		.format = description.format,
		.extent = vk::Extent3D(description.width, description.height, description.depth),
		.mipLevels = description.miplevels,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.usage = description.usage,
		.initialLayout = vk::ImageLayout::eUndefined,
	};

	vk::Image image = device.createImage(imageInfo);

	return image;
}

ResourceManager::AllocationIndex ResourceManager::createResources(
	const std::vector<ImageDescription> &imagesDescriptions,
	const std::vector<BufferDescription> &buffersDescriptions,
	ResourceManager::MemoryLocation location
) {
	assert(!(imagesDescriptions.empty() && buffersDescriptions.empty()));

	assert(std::find_if(buffersDescriptions.begin(), buffersDescriptions.end(), [](const BufferDescription &desc) {
			   return desc.size == 0;
		   }) == buffersDescriptions.end());

	vk::Device &device = Instance::Get().device;

	uint32_t requiredSize = 0;

	std::vector<ImageHandle> images;
	std::vector<BufferHandle> buffers;
	std::vector<vk::MemoryRequirements> resourcesRequirements;

	resourcesRequirements.reserve(imagesDescriptions.size() + buffersDescriptions.size());

	AllocationIndex allocIndex = m_allocations.size();

	for (int i = 0; i < imagesDescriptions.size(); i++) {
		auto &description = imagesDescriptions[i];

		vk::Image image = createImage(description);

		vk::MemoryRequirements requirements = device.getImageMemoryRequirements(image);

		ImageHandle handle = { static_cast<ImageHandle>(m_images.size()) };

		requiredSize = (requiredSize + requirements.alignment - 1) & ~(requirements.alignment - 1);
		requiredSize += requirements.size;

		resourcesRequirements.push_back(requirements);
		images.push_back(handle);

		m_images.push_back({
			.image = image,
			.format = description.format,
			.size = { description.width,
                     description.height,
                     description.depth,
					},
			.mipLevels = description.miplevels,
		});

		m_allocationImages[allocIndex].push_back(handle);
	}
	for (int i = 0; i < buffersDescriptions.size(); i++) {
		auto &description = buffersDescriptions[i];

		uint32_t bufferSize = description.size;

		vk::Buffer buffer = device.createBuffer(
			vk::BufferCreateInfo {
				.size = bufferSize,
				.usage = description.usage,
			}
		);

		vk::MemoryRequirements requirements = device.getBufferMemoryRequirements(buffer);

		requiredSize = (requiredSize + requirements.alignment - 1) & ~(requirements.alignment - 1);
		requiredSize += requirements.size;

		resourcesRequirements.push_back(requirements);

		BufferHandle handle = { (uint32_t)m_buffers.size() };

		buffers.push_back(handle);
		m_buffers.push_back(
			Buffer {
				.buffer = buffer,
				.size = bufferSize,
			}
		);
		m_allocationBuffers[allocIndex].push_back(handle);
	}

	vk::MemoryPropertyFlags locationFlag;
	switch (location) {
		case ResourceManager::MemoryLocation::Device:
			locationFlag = vk::MemoryPropertyFlagBits::eDeviceLocal;
			break;
		case ResourceManager::MemoryLocation::HostVisible:
			locationFlag = vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible;
			break;
		case ResourceManager::MemoryLocation::Host:
			locationFlag = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
			break;
	}

	m_allocations.push_back(LinearAllocator(locationFlag, requiredSize));
	LinearAllocator &allocation = m_allocations.at(allocIndex);
	for (int i = 0; i < images.size(); i++) {
		Image &image = m_images.at(images.at(i).value);
		image.allocation = m_allocations.at(allocIndex).subAllocate(resourcesRequirements.at(i));

		device.bindImageMemory(
			image.image, m_allocations.at(allocIndex).getAllocation().memory, image.allocation->offset
		);

		image.views.push_back(device.createImageView( {
    		.image = image.image,
    		.viewType = image.size.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
    		.format = image.format,
    		.subresourceRange = { .aspectMask =
    								Image::GetAspectFlags(image.format),
    							.baseMipLevel = 0,
    							.levelCount = image.mipLevels ,
    							.baseArrayLayer = 0,
    							.layerCount = 1,
    							},
    	}));

		if (Image::GetAspectFlags(image.format) & vk::ImageAspectFlagBits::eStencil) {
			image.views.push_back(device.createImageView( {
    		.image = image.image,
    		.viewType = image.size.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
    		.format = image.format,
    		.subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eDepth,
    							.baseMipLevel = 0,
    							.levelCount = image.mipLevels ,
    							.baseArrayLayer = 0,
    							.layerCount = 1,
    							},
    	    }));
		}

		if (image.mipLevels > 1) {
			for (int mip = 0; mip < imagesDescriptions[i].miplevels; mip++) {
				image.views.push_back(device.createImageView( {
              		.image = image.image,
              		.viewType = image.size.depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D,
              		.format = image.format,
              		.subresourceRange = { .aspectMask = Image::GetAspectFlags(image.format),
             							.baseMipLevel = (uint32_t)mip,
             							.levelCount = 1,
             							.baseArrayLayer = 0,
             							.layerCount = 1,
             							},
           	    }));
			}
		}
	}
	for (int i = 0; i < buffers.size(); i++) {
		Buffer &buffer = m_buffers[buffers[i].value];
		buffer.allocation = m_allocations.at(allocIndex).subAllocate(resourcesRequirements.at(images.size() + i));

		device.bindBufferMemory(
			buffer.buffer, m_allocations.at(allocIndex).getAllocation().memory, buffer.allocation.offset
		);
	}

	if (location != MemoryLocation::Device) {
		void *address = allocation.getAllocation().address;

		for (BufferHandle bufferHandle : buffers) {
			Buffer &buffer = m_buffers[bufferHandle.value];
			buffer.data = static_cast<std::byte *>(address) + buffer.allocation.offset;
		}
	}

	return allocIndex;
}

void ResourceManager::freeAllocation(ResourceManager::AllocationIndex index) {
	vk::Device &device = Instance::Get().device;

	for (auto &image : m_allocationImages[index]) {
		for (auto view : m_images[image.value].views) device.destroyImageView(view);
		device.destroyImage(m_images[image.value].image);
	}

	for (auto &buffer : m_allocationBuffers[index]) {
		device.destroyBuffer(m_buffers[buffer.value].buffer);
	}
	m_allocationBuffers.erase(index);
	m_allocationImages.erase(index);

	m_allocations.at(index).getAllocation().free();
}

void writeCopy(
	vk::CommandBuffer &commandBuffer,
	const std::vector<Image> &images,
	const std::vector<Buffer> &buffers,
	const ResourceManager::ResourceCopyInfo &copyInfo
) {
	bool bufferSource = std::holds_alternative<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.source);

	bool bufferDestination =
		std::holds_alternative<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.destination);
	bool imageSource = !bufferSource;
	bool imageDestination = !bufferDestination;

	if (bufferSource && bufferDestination) {
		auto &srcInfo = std::get<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.source);
		auto &dstInfo = std::get<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.destination);

		vk::BufferCopy2 region {
			.srcOffset = srcInfo.offset,
			.dstOffset = dstInfo.offset,
			.size = srcInfo.size,
		};

		commandBuffer.copyBuffer2(
			{
				.srcBuffer = buffers[srcInfo.handle.value].buffer,
				.dstBuffer = buffers[dstInfo.handle.value].buffer,
				.regionCount = 1,
				.pRegions = &region,
			}
		);
	} else if (bufferSource && imageDestination) {
		auto &srcInfo = std::get<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.source);
		auto &dstInfo = std::get<ResourceManager::ResourceCopyInfo::ImageReference>(copyInfo.destination);
		const Image &destImage = images[dstInfo.handle.value];

		vk::Extent3D mipExtent = destImage.size;

		mipExtent.width /= 1 << dstInfo.mipLevel;
		mipExtent.height /= 1 << dstInfo.mipLevel;

		vk::BufferImageCopy2 region {
			.bufferOffset = srcInfo.offset,
			.imageSubresource = {
			    .aspectMask = destImage.getAspectFlags(),
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
			.image = destImage.image,
			.subresourceRange = {
        		.aspectMask = destImage.getAspectFlags(),
          		.baseMipLevel = dstInfo.mipLevel,
          		.levelCount = 1,
          		.baseArrayLayer = 0,
          		.layerCount = 1,
    		},
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &preTransferBarrier,
			}
		);

		commandBuffer.copyBufferToImage2(
			vk::CopyBufferToImageInfo2 {
				.srcBuffer = buffers[srcInfo.handle.value].buffer,
				.dstImage = images[dstInfo.handle.value].image,
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
			.image = destImage.image,
			.subresourceRange = {
        		.aspectMask = destImage.getAspectFlags(),
          		.baseMipLevel = dstInfo.mipLevel,
          		.levelCount = 1,
          		.baseArrayLayer = 0,
          		.layerCount = 1,
    		},
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &postTransferBarrier,
			}
		);

	} else if (imageSource && bufferDestination) {
		auto &srcInfo = std::get<ResourceManager::ResourceCopyInfo::ImageReference>(copyInfo.source);
		auto &dstInfo = std::get<ResourceManager::ResourceCopyInfo::BufferReference>(copyInfo.destination);
		const Image &srcImage = images[srcInfo.handle.value];
		vk::Extent3D mipExtent = srcImage.size;

		mipExtent.width /= 1 << srcInfo.mipLevel;
		mipExtent.height /= 1 << srcInfo.mipLevel;

		vk::ImageMemoryBarrier2 preTransferBarrier {
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = srcInfo.initialLayout,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.image = srcImage.image,
			.subresourceRange = {
        		.aspectMask = srcImage.getAspectFlags(),
          		.baseMipLevel = srcInfo.mipLevel,
          		.levelCount = 1,
          		.baseArrayLayer = 0,
          		.layerCount = 1,
    		},
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &preTransferBarrier,
			}
		);

		vk::BufferImageCopy2 region {
			.bufferOffset = dstInfo.offset,
			.imageSubresource = {
			    .aspectMask = srcImage.getAspectFlags(),
                .mipLevel = srcInfo.mipLevel,
                .baseArrayLayer = 0,
                .layerCount = 1,
			},
			.imageExtent = mipExtent,
		};
		commandBuffer.copyImageToBuffer2(
			vk::CopyImageToBufferInfo2 {
				.srcImage = srcImage.image,
				.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
				.dstBuffer = buffers[dstInfo.handle.value].buffer,
				.regionCount = 1,
				.pRegions = &region,
			}
		);

		vk::ImageMemoryBarrier2 postTransferBarrier {
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = srcInfo.finalLayout,
			.image = srcImage.image,
			.subresourceRange = {
        		.aspectMask = srcImage.getAspectFlags(),
          		.baseMipLevel = srcInfo.mipLevel,
          		.levelCount = 1,
          		.baseArrayLayer = 0,
          		.layerCount = 1,
    		},
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers = &postTransferBarrier,
			}
		);
	} else if (imageSource && imageDestination) {
		auto &srcInfo = std::get<ResourceManager::ResourceCopyInfo::ImageReference>(copyInfo.source);
		auto &dstInfo = std::get<ResourceManager::ResourceCopyInfo::ImageReference>(copyInfo.destination);
		const Image &srcImage = images[srcInfo.handle.value];
		const Image &dstImage = images[dstInfo.handle.value];

		vk::Extent3D mipExtent = srcImage.size;

		mipExtent.width /= 1 << srcInfo.mipLevel;
		mipExtent.height /= 1 << srcInfo.mipLevel;

		std::array<vk::ImageMemoryBarrier2, 2> preTransferBarrier {
			vk::ImageMemoryBarrier2 {
									 .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
									 .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
									 .oldLayout = srcInfo.initialLayout,
									 .newLayout = vk::ImageLayout::eTransferSrcOptimal,
									 .image = srcImage.image,
										.subresourceRange = {
                                      		.aspectMask = srcImage.getAspectFlags(),
                                      		.baseMipLevel = srcInfo.mipLevel,
                                      		.levelCount = 1,
                                      		.baseArrayLayer = 0,
                                      		.layerCount = 1,
                                  		},
									 },
			vk::ImageMemoryBarrier2 {
									 .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
									 .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
									 .oldLayout = dstInfo.initialLayout,
									 .newLayout = vk::ImageLayout::eTransferDstOptimal,
									 .image = dstImage.image,
									.subresourceRange = {
                                  		.aspectMask = dstImage.getAspectFlags(),
                                  		.baseMipLevel = dstInfo.mipLevel,
                                  		.levelCount = 1,
                                  		.baseArrayLayer = 0,
                                  		.layerCount = 1,
                              		},
								}
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 2,
				.pImageMemoryBarriers = preTransferBarrier.data(),
			}
		);
		vk::ImageCopy2 region {
		    .srcSubresource = {
			    .aspectMask = srcImage.getAspectFlags(),
                .mipLevel = srcInfo.mipLevel,
                .baseArrayLayer = 0,
                .layerCount = 1,
			},
            .dstSubresource = {
                .aspectMask = srcImage.getAspectFlags(),
                .mipLevel = srcInfo.mipLevel,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .extent = mipExtent,
		};
		commandBuffer.copyImage2(
			vk::CopyImageInfo2 {
				.srcImage = srcImage.image,
				.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
				.dstImage = dstImage.image,
				.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
				.regionCount = 1,
				.pRegions = &region,
			}
		);

		std::array<vk::ImageMemoryBarrier2, 2> postTransferBarrier {

			vk::ImageMemoryBarrier2 {
    								.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
    								.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
    								.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
    								.newLayout = srcInfo.finalLayout,
    								.image = srcImage.image,
									.subresourceRange = {
                                  		.aspectMask = srcImage.getAspectFlags(),
                                  		.baseMipLevel = srcInfo.mipLevel,
                                  		.levelCount = 1,
                                  		.baseArrayLayer = 0,
                                  		.layerCount = 1,
                              		},
								},
			vk::ImageMemoryBarrier2 {
									.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
									.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
									.oldLayout = vk::ImageLayout::eTransferDstOptimal,
									.newLayout = dstInfo.finalLayout,
									.image = dstImage.image,
										.subresourceRange = {
                                  		.aspectMask = dstImage.getAspectFlags(),
                                  		.baseMipLevel = dstInfo.mipLevel,
                                  		.levelCount = 1,
                                  		.baseArrayLayer = 0,
                                  		.layerCount = 1,
                              		},
								}
		};

		commandBuffer.pipelineBarrier2(
			vk::DependencyInfo {
				.imageMemoryBarrierCount = 2,
				.pImageMemoryBarriers = postTransferBarrier.data(),
			}
		);
	}
}

void ResourceManager::copyResources(const std::vector<ResourceCopyInfo> &info) {
	auto &device = Instance::Get().device;

	if (device.getSemaphoreCounterValue(m_semaphore) == m_transferCount) device.resetCommandPool(m_commandPool);

	vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(
		{
			.commandPool = m_commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		}
	)[0];
	vk::CommandBufferBeginInfo beginInfo {
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
	};

	commandBuffer.begin(beginInfo);

	for (const auto &copyInfo : info) writeCopy(commandBuffer, m_images, m_buffers, copyInfo);

	commandBuffer.end();

	m_transferCount += 1;
	vk::TimelineSemaphoreSubmitInfo semaphoreSignalInfo {
		.signalSemaphoreValueCount = 1,
		.pSignalSemaphoreValues = &m_transferCount,
	};

	vk::SubmitInfo submitInfo {
		.pNext = &semaphoreSignalInfo,
		.commandBufferCount = 1,
		.pCommandBuffers = &commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_semaphore,
	};
	auto _ = Instance::Get().transferQueue.submit(1, &submitInfo, nullptr);
}
