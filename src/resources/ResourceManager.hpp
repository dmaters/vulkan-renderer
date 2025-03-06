#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "Buffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "MemoryAllocator.hpp"

class ResourceManager {
public:
	struct ImageDescription;
	struct BufferDescription;

private:
	vk::Device& m_device;
	MemoryAllocator m_memoryAllocator;

public:
	ResourceManager(Instance& instance);

	Buffer createBuffer(const BufferDescription& description);
	Image createImage(const ImageDescription& description);
	Image loadImage(const std::filesystem::path& path);
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
	MemoryAllocator::Location location;
};