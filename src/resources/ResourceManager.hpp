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

struct ResourceUsage {
	enum class Type : uint8_t {
		WRITE = 0,
		READ_WRITE = 1,
		READ = 2,
	};
	Type type;
	vk::AccessFlags2 access;
	vk::PipelineStageFlags2 stage;

	bool operator==(const ResourceUsage& b) const {
		return (type == Type::READ && b.type == ResourceUsage::Type::READ) ||
		       (type == Type::WRITE && b.type == ResourceUsage::Type::WRITE);
	}
};

class ResourceManager {
public:
	struct ImageDescription;
	struct BufferDescription;
	struct BufferCopy;

	static const std::unordered_map<std::string_view, BufferDescription>
		_defaultNamedBufferData;
	static const std::unordered_map<std::string_view, ImageDescription>
		_defaultNamedImageData;

	static const std::unordered_map<std::string_view, int8_t> _swapchainRatio;

private:
	MemoryAllocator& m_memoryAllocator;

	vk::CommandPool m_commandPool;
	vk::Queue m_queue;
	std::unordered_map<uint32_t, Image> m_images;
	std::unordered_map<uint32_t, Buffer> m_buffers;
	uint32_t m_resourceCounter = 0;

public:
	ResourceManager(Instance& instance, MemoryAllocator& memoryAllocator);

	BufferHandle createStagingBuffer(uint32_t size);
	BufferHandle createBuffer(const BufferDescription& description);

	ImageHandle createImage(const ImageDescription& description);

	ImageHandle loadImage(const std::filesystem::path& path);

	Image& getImage(ImageHandle handle) { return m_images[handle.value]; }

	Buffer& getBuffer(BufferHandle handle) { return m_buffers[handle.value]; }

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
	uint32_t miplevels = 1;
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
