#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <unordered_map>
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
#include "resources/Buffer.hpp"

struct BufferHandle {
	uint32_t value;
};

struct ImageHandle {
	uint32_t value;
};

class ResourceManager {
public:
	struct ImageDescription;
	struct BufferDescription;
	struct BufferCopy;

	using DeviceAllocationIndex = uint32_t;
	using HostAllocationIndex = uint32_t;

private:
	vk::Semaphore m_semaphore;
	uint64_t m_transferCount = 0;
	uint32_t m_resourceCounter = 0;

	vk::CommandPool m_transferPool;
	vk::CommandPool m_graphicPool;

	std::unordered_map<uint32_t, Image> m_images;
	std::unordered_map<uint32_t, Buffer> m_buffers;

	std::unordered_map<std::string_view, ImageHandle> m_imageNames;
	std::unordered_map<std::string_view, BufferHandle> m_bufferNames;

	std::vector<LinearAllocator> m_stagingAdditionalAllocators;
	vk::CommandBuffer m_stagingCommandBuffer;
	RingAllocator m_stagingAllocation;
	vk::Buffer m_stagingBuffer;

	std::unordered_map<DeviceAllocationIndex, LinearAllocator>
		m_deviceAllocations;
	std::unordered_map<HostAllocationIndex, Allocation> m_hostAllocations;

	std::unordered_map<
		DeviceAllocationIndex,
		std::pair<std::vector<ImageHandle>, std::vector<BufferHandle>>>
		m_deviceAllocationResources;
	std::unordered_map<HostAllocationIndex, std::vector<BufferHandle>>
		m_hostAllocationBuffers;
	std::unordered_map<HostAllocationIndex, void*> m_hostAllocationAddresses;

	uint32_t m_allocationCount = 0;

	ImageHandle registerImage(Image image, ImageHandle handle = { 0 });

	void copyToBuffer(const std::vector<std::byte>& bytes);
	void copyBuffers(std::vector<BufferCopy>& info);

public:
	ResourceManager();
	Image& getImage(ImageHandle handle) { return m_images.at(handle.value); }
	Buffer& getBuffer(BufferHandle handle) {
		return m_buffers.at(handle.value);
	}

	const std::vector<ImageHandle>& getImages(
		DeviceAllocationIndex index
	) const {
		if (index == 0) return {};
		return m_deviceAllocationResources.at(index).first;
	}
	const std::vector<BufferHandle>& getBuffers(
		DeviceAllocationIndex index
	) const {
		if (index == 0) return {};
		return m_deviceAllocationResources.at(index).second;
	}
	const std::vector<BufferHandle>& getHostBuffers(
		HostAllocationIndex index
	) const {
		if (index == 0) return {};
		return m_hostAllocationBuffers.at(index);
	}
	void* getHostAllocationAddress(HostAllocationIndex index) const {
		return m_hostAllocationAddresses.at(index);
	}

	ImageHandle getNamedImageIndex(std::string_view name) const {
		return m_imageNames.at(name);
	}
	BufferHandle getNamedBufferIndex(std::string_view name) const {
		return m_bufferNames.at(name);
	}

	Image& getNamedImage(std::string_view name) {
		return m_images.at(m_imageNames.at(name).value);
	}
	Buffer& getNamedBuffer(std::string_view name) {
		return m_buffers.at(m_bufferNames.at(name).value);
	}

	void setName(std::string_view name, ImageHandle image);
	void setName(std::string_view name, BufferHandle buffer);

	struct TextureInfo {
		std::filesystem::path path;
		vk::Format expectedFormat;
	};

	DeviceAllocationIndex loadSceneTextures(std::vector<TextureInfo> textures);
	DeviceAllocationIndex createResources(
		std::vector<ImageDescription> images,
		std::vector<BufferDescription> buffers
	);
	HostAllocationIndex createHostAllocation(std::vector<uint32_t> sizes);

	void freeDeviceAllocation(DeviceAllocationIndex index);
	void freeHostAllocation(HostAllocationIndex index);

	template <typename T, typename F>
		requires std::invocable<F, T&>
	void queueBufferUpdate(BufferHandle handle, F updateFunction);

	template <typename F>
		requires std::invocable<F, std::byte*>
	void queueBufferUpdate(BufferHandle handle, F updateFunction);

	const vk::Semaphore& getSemaphore() const { return m_semaphore; }
	uint64_t sync();
};

struct ResourceManager::ImageDescription {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t miplevels = 1;
	vk::Format format;
	vk::ImageUsageFlags usage;
};
struct ResourceManager::BufferDescription {
	uint32_t size;
	vk::BufferUsageFlags usage;
};

struct ResourceManager::BufferCopy {
	vk::Buffer origin;
	vk::Buffer destination;
	vk::BufferCopy copy;
};

template <typename T, typename F>
	requires std::invocable<F, T&>
void ResourceManager::queueBufferUpdate(BufferHandle handle, F updateFunction) {
	Buffer& buffer = getBuffer(handle);
	// assert(buffer.size == sizeof(T));

	vk::Device& device = Instance::Get().device;
	vk::MemoryRequirements requirements =
		device.getBufferMemoryRequirements(buffer.buffer);
	requirements.alignment = 0;

	SubAllocation stagingAllocation =
		m_stagingAllocation.subAllocate(requirements);

	updateFunction(*(T*)(m_stagingAllocation.getAllocation().address +
	                     stagingAllocation.offset));

	std::vector<ResourceManager::BufferCopy> info = {
			{
				.origin = m_stagingBuffer,
				.destination = m_buffers[handle.value].buffer,
				.copy = {
					.srcOffset = stagingAllocation.offset,
					.size = buffer.size,
				 },
			}
		};

	copyBuffers(info);
}

template <typename F>
	requires std::invocable<F, std::byte*>
void ResourceManager::queueBufferUpdate(BufferHandle handle, F updateFunction) {
	Buffer& buffer = getBuffer(handle);

	vk::Device& device = Instance::Get().device;
	vk::MemoryRequirements requirements =
		device.getBufferMemoryRequirements(buffer.buffer);
	requirements.alignment = 0;
	SubAllocation stagingAllocation =
		m_stagingAllocation.subAllocate(requirements);

	updateFunction(
		(m_stagingAllocation.getAllocation().address + stagingAllocation.offset)
	);

	std::vector<ResourceManager::BufferCopy> info = {
		{
         .origin = m_stagingBuffer,
         .destination = m_buffers[handle.value].buffer,
         .copy = { .srcOffset= stagingAllocation.offset, .size = buffer.size, },
		 }
	};

	copyBuffers(info);
}
