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

ResourceManager::ResourceManager() {}
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
