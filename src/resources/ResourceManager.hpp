#pragma once

#include <cstdint>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Buffer.hpp"
#include "Image.hpp"
#include "memory/LinearAllocator.hpp"
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

	using AllocationIndex = uint32_t;

private:
	std::vector<Image> m_images;
	std::vector<Buffer> m_buffers;

	std::vector<LinearAllocator> m_allocations;
	std::unordered_map<AllocationIndex, std::vector<ImageHandle>>
		m_allocationImages;
	std::unordered_map<AllocationIndex, std::vector<BufferHandle>>
		m_allocationBuffers;

	vk::CommandPool m_commandPool;
	vk::Semaphore m_semaphore;
	std::size_t m_transferCount = 0;

	ImageHandle registerImage(Image image, ImageHandle handle = { 0 });

public:
	ResourceManager();

	Image& getImage(ImageHandle handle) { return m_images.at(handle.value); }
	Buffer& getBuffer(BufferHandle handle) {
		return m_buffers.at(handle.value);
	}

	const Image& getImage(ImageHandle handle) const {
		return m_images[handle.value];
	}
	const Buffer& getBuffer(BufferHandle handle) const {
		return m_buffers[handle.value];
	}

	const std::span<const ImageHandle> getImages(AllocationIndex index) const {
		if (!m_allocationImages.contains(index))
			return std::span<ImageHandle, 0>();
		auto& images = m_allocationImages.at(index);
		return std::span<const ImageHandle>(images);
	}
	const std::span<const BufferHandle> getBuffers(
		AllocationIndex index
	) const {
		if (!m_allocationBuffers.contains(index))
			return std::span<BufferHandle, 0>();

		auto& buffers = m_allocationBuffers.at(index);
		return std::span<const BufferHandle>(buffers);
	}
	void setName(std::string name, ImageHandle image);
	void setName(std::string name, BufferHandle buffer);

	enum class MemoryLocation {
		Device,
		HostVisible,
		Host,
	};
	AllocationIndex createResources(
		const std::vector<ImageDescription>& images,
		const std::vector<BufferDescription>& buffers,
		MemoryLocation location
	);
	void freeAllocation(AllocationIndex index);

	struct ResourceCopyInfo;
	void copyResources(const std::vector<ResourceCopyInfo>& info);

	struct SemaphoreInfo {
		vk::Semaphore semaphore;
		std::size_t expectedValue;
	};
	SemaphoreInfo getSemaphoreInfo() const {
		return {
			.semaphore = m_semaphore,
			.expectedValue = m_transferCount,
		};
	}
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

struct ResourceManager::ResourceCopyInfo {
	struct BufferReference {
		BufferHandle handle;
		uint32_t size;
		uint32_t offset;
	};
	struct ImageReference {
		ImageHandle handle;
		uint32_t mipLevel = 0;
		vk::ImageLayout initialLayout;
		vk::ImageLayout finalLayout;
	};

	using ResourceReference = std::variant<ImageReference, BufferReference>;

	ResourceReference source;
	ResourceReference destination;
};
