#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vulkan/vulkan.hpp>

#include "Buffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "memory/MemoryAllocator.hpp"

class ResourceManager {
public:
	struct ImageDescription;
	struct BufferDescription;

private:
	vk::Device m_device;

	vk::CommandPool m_commandPool;
	vk::Queue m_queue;
	MemoryAllocator m_memoryAllocator;

public:
	ResourceManager(Instance& instance);

	Buffer createStagingBuffer(size_t size);
	Buffer createBuffer(const BufferDescription& description);

	Image createImage(const ImageDescription& description);
	Image loadImage(const std::filesystem::path& path);

	void copyToBuffer(const std::vector<std::byte>& bytes, Buffer& buffer);

	void copyBuffer(Buffer& origin, Buffer& destination, vk::BufferCopy offset);
	void copyToImage(
		Buffer& origin, Image& destination, vk::BufferImageCopy offset
	);
	void free(Buffer buffer) {}
	void free(Image image) {}
};

struct ResourceManager::ImageDescription {
	uint32_t width;
	uint32_t height;
	vk::Format format;
	vk::ImageUsageFlags usage;
	std::optional<vk::ClearColorValue> clearColor;
};

struct ResourceManager::BufferDescription {
	size_t size;
	vk::BufferUsageFlags usage;
	AllocationLocation location;
};