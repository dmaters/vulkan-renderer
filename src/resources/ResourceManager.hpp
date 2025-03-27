#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string_view>
#include <vulkan/vulkan.hpp>

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

private:
	vk::Device m_device;
	vk::CommandPool m_commandPool;
	vk::Queue m_queue;
	MemoryAllocator& m_memoryAllocator;
	std::map<uint32_t, Image> m_images;
	std::map<uint32_t, Buffer> m_buffers;
	std::map<std::string_view, uint32_t> m_imageNames;
	std::map<std::string_view, uint32_t> m_bufferNames;

	// TODO: indexing, for now we can't initialize more than 2^32 resources
	uint32_t m_resourceCounter = 0;

public:
	ResourceManager(Instance& instance, MemoryAllocator& memoryAllocator);

	BufferHandle createStagingBuffer(uint32_t size);
	BufferHandle createBuffer(const BufferDescription& description);
	inline BufferHandle createBuffer(
		std::string_view name, const BufferDescription& description
	) {
		BufferHandle handle = createBuffer(description);
		setName(name, handle);
		return handle;
	}

	ImageHandle createImage(const ImageDescription& description);
	inline ImageHandle createImage(
		std::string_view name, const ImageDescription& description
	) {
		ImageHandle handle = createImage(description);
		setName(name, handle);
		return handle;
	}

	ImageHandle loadImage(const std::filesystem::path& path);

	inline Image& getNamedImage(std::string_view name) {
		assert(m_imageNames.contains(name));
		return m_images[m_imageNames[name]];
	}
	inline Buffer& getNamedBuffer(std::string_view name) {
		assert(m_bufferNames.contains(name));
		return m_buffers[m_bufferNames[name]];
	}
	inline Image& getImage(ImageHandle handle) {
		return m_images[handle.value];
	}
	inline Buffer& getBuffer(BufferHandle handle) {
		return m_buffers[handle.value];
	}

	inline void setName(std::string_view name, ImageHandle handle) {
		m_imageNames[name] = handle.value;
	}
	inline void setName(std::string_view name, BufferHandle handle) {
		m_bufferNames[name] = handle.value;
	}

	ImageHandle registerImage(Image image);

	void copyToBuffer(const std::vector<std::byte>& bytes, BufferHandle);

	void copyBuffer(
		BufferHandle source, BufferHandle destination, vk::BufferCopy offset
	);
	void copyToImage(
		BufferHandle origin, ImageHandle destination, vk::BufferImageCopy offset
	);

	void free(BufferHandle buffer) {}
	void free(ImageHandle buffer) {}
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