#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>

#include "Buffer.hpp"
#include "Image.hpp"
#include "Instance.hpp"
#include "MemoryAllocator.hpp"

template <typename T>
class Handle {
private:
	static uint32_t s_handlesCreated;
	uint32_t value = 0;

public:
	static const T UNASSIGNED;

	bool operator==(const Handle<T>& other) const {
		return value == other.value;
	}
	bool operator<(const Handle<T>& other) const { return value < other.value; }

	Handle<T>() : value(++s_handlesCreated) {}
};

template <typename T>
const T Handle<T>::UNASSIGNED = { 0 };
template <typename T>
uint32_t Handle<T>::s_handlesCreated = 0;

class ResourceManager {
public:
	class ImageHandle : public Handle<ImageHandle> {
	private:
		ResourceManager& m_manager;

	public:
		ImageHandle(ResourceManager& manager) : m_manager(manager) {}
		inline Image& get() { return m_manager.getImage(*this); }
	};

	class BufferHandle : public Handle<BufferHandle> {
	private:
		ResourceManager& m_manager;

	public:
		BufferHandle(ResourceManager& manager) : m_manager(manager) {}
		inline Buffer& get() { return m_manager.getBuffer(*this); }
	};

	struct ImageDescription;
	struct BufferDescription;

private:
	vk::Device& m_device;
	MemoryAllocator m_memoryAllocator;
	std::map<ImageHandle, Image> m_images;
	std::map<std::filesystem::path, ImageHandle> m_imageCache;
	std::map<BufferHandle, Buffer> m_buffers;

public:
	ResourceManager(Instance& instance);

	BufferHandle createBuffer(size_t size);
	ImageHandle createImage(const ImageDescription& description);
	ImageHandle loadImage(const std::filesystem::path& path);

	inline Image& getImage(ImageHandle handle) { return m_images.at(handle); }
	inline Buffer& getBuffer(BufferHandle handle) {
		return m_buffers.at(handle);
	}
};

struct ResourceManager::ImageDescription {
	uint32_t width;
	uint32_t height;
	vk::Format format;
	vk::ImageUsageFlags usage;
};