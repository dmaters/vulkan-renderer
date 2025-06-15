#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
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
#include "memory/MemoryAllocator.hpp"
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

private:
	vk::Device m_device;
	vk::CommandPool m_commandPool;
	vk::Queue m_queue;

	uint32_t m_transferIndex = 0;
	uint32_t m_graphicsIndex = 0;

	MemoryAllocator& m_memoryAllocator;
	std::unordered_map<uint32_t, Image> m_images;
	std::unordered_map<uint32_t, Buffer> m_buffers;
	std::unordered_map<std::string_view, uint32_t> m_imageNames;
	std::unordered_map<std::string_view, uint32_t> m_bufferNames;

	// TODO: indexing, for now we can't initialize more than 2^32 resources
	uint32_t m_resourceCounter = 0;

public:
	ResourceManager(Instance& instance, MemoryAllocator& memoryAllocator);

	BufferHandle createStagingBuffer(uint32_t size);
	BufferHandle createBuffer(const BufferDescription& description);
	BufferHandle createBuffer(
		std::string_view name, const BufferDescription& description
	) {
		BufferHandle handle = createBuffer(description);
		setName(name, handle);
		return handle;
	}

	ImageHandle createImage(const ImageDescription& description);
	ImageHandle createImage(
		std::string_view name, const ImageDescription& description
	) {
		ImageHandle handle = createImage(description);
		setName(name, handle);
		return handle;
	}

	ImageHandle loadImage(const std::filesystem::path& path);

	Image& getNamedImage(std::string_view name) {
		assert(m_imageNames.contains(name));
		return m_images[m_imageNames[name]];
	}
	ImageHandle getNamedImageHandle(std::string_view name) {
		return ImageHandle { m_imageNames[name] };
	}
	Buffer& getNamedBuffer(std::string_view name) {
		assert(m_bufferNames.contains(name));
		return m_buffers[m_bufferNames[name]];
	}
	BufferHandle getNamedBufferHandle(std::string_view name) {
		return BufferHandle { m_bufferNames[name] };
	}
	Image& getImage(ImageHandle handle) { return m_images[handle.value]; }

	Buffer& getBuffer(BufferHandle handle) { return m_buffers[handle.value]; }

	void setName(std::string_view name, ImageHandle handle) {
		m_imageNames[name] = handle.value;
	}
	void setName(std::string_view name, BufferHandle handle) {
		m_bufferNames[name] = handle.value;
	}

	ImageHandle registerImage(Image image);

	void copyToBuffer(const std::vector<std::byte>& bytes, BufferHandle);

	void copyBuffers(std::vector<BufferCopy>& info);
	void copyToImage(
		BufferHandle origin, ImageHandle destination, vk::BufferImageCopy offset
	);

	void free(BufferHandle buffer) {
		m_memoryAllocator.free(m_buffers[buffer.value].allocation);
		m_buffers.erase(buffer.value);
	}
	void free(ImageHandle image) {
		m_memoryAllocator.free(m_images[image.value].allocation.value());
		m_images.erase(image.value);
	}
};

struct ResourceManager::ImageDescription {
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	vk::Format format;
	vk::ImageUsageFlags usage;
	bool transient = false;
};

struct ResourceManager::BufferDescription {
	uint32_t size;
	vk::BufferUsageFlags usage;
	AllocationLocation location;
	bool transient = false;
};

struct ResourceManager::BufferCopy {
	BufferHandle origin;
	uint8_t originAccessIndex = 0;
	BufferHandle destination;
	uint8_t destinationAccessIndex = 0;
	vk::BufferCopy copy;
};
