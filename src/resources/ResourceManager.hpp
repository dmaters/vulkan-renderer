#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
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

	typedef uint32_t AllocationIndex;

private:
	vk::Semaphore m_semaphore;
	uint64_t m_transferCount = 0;
	uint32_t m_resourceCounter = 0;

	vk::CommandPool m_commandPool;
	std::unordered_map<uint32_t, Image> m_images;
	std::unordered_map<uint32_t, Buffer> m_buffers;

	std::unordered_map<std::string_view, ImageHandle> m_imageNames;
	std::unordered_map<std::string_view, BufferHandle> m_bufferNames;

	RingAllocator m_stagingAllocation;
	std::unordered_map<AllocationIndex, LinearAllocator> m_allocations;
	std::unordered_map<
		AllocationIndex,
		std::pair<std::vector<ImageHandle>, std::vector<BufferHandle>>>
		m_allocationResources;
	uint32_t m_allocationCount = 0;

	ImageHandle registerImage(Image image, ImageHandle handle = { 0 });

	void copyToBuffer(const std::vector<std::byte>& bytes);
	void copyBuffers(std::vector<BufferCopy>& info);
	void copyToImage(
		vk::Buffer origin, vk::Image destination, vk::BufferImageCopy offset
	);

public:
	ResourceManager();
	Image& getImage(ImageHandle handle) { return m_images.at(handle.value); }
	Buffer& getBuffer(BufferHandle handle) {
		return m_buffers.at(handle.value);
	}

	const std::vector<ImageHandle> getImages(AllocationIndex index) const {
		if (index == 0) return {};
		return m_allocationResources.at(index).first;
	}
	const std::vector<BufferHandle> getBuffers(AllocationIndex index) const {
		if (index == 0) return {};
		return m_allocationResources.at(index).second;
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

	AllocationIndex loadSceneTextures(std::vector<TextureInfo> textures);
	AllocationIndex createResources(
		std::vector<ImageDescription> images,
		std::vector<BufferDescription> buffers
	);

	template <typename T, typename F>
		requires std::invocable<F, T&>
	void updateBufferSync(BufferHandle handle, F updateFunction);

	template <typename F>
		requires std::invocable<F, std::byte*>
	void updateBufferSync(BufferHandle handle, F updateFunction);
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
void ResourceManager::updateBufferSync(BufferHandle handle, F updateFunction) {
	Buffer& buffer = getBuffer(handle);
	// assert(buffer.size == sizeof(T));

	vk::Device& device = Instance::Get().device;
	vk::Buffer stagingBuffer = device.createBuffer(vk::BufferCreateInfo {
		.size = buffer.size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	});

	SubAllocation stagingAllocation = m_stagingAllocation.subAllocate(
		device.getBufferMemoryRequirements(stagingBuffer)
	);

	device.bindBufferMemory(
		stagingBuffer,
		m_stagingAllocation.getAllocation().memory,
		stagingAllocation.offset
	);
	std::byte* base =
		static_cast<std::byte*>(m_stagingAllocation.getAllocation().address);

	T* ptr =
		std::launder(reinterpret_cast<T*>(base + stagingAllocation.offset));
	updateFunction(*ptr);

	std::vector<ResourceManager::BufferCopy> info = {
			{
				.origin = stagingBuffer,
				.destination = m_buffers[handle.value].buffer,
				.copy = {
					 .size = buffer.size,
				 },
			}
		};

	copyBuffers(info);
	device.destroyBuffer(stagingBuffer);
}

template <typename F>
	requires std::invocable<F, std::byte*>
void ResourceManager::updateBufferSync(BufferHandle handle, F updateFunction) {
	Buffer& buffer = getBuffer(handle);

	vk::Device& device = Instance::Get().device;
	vk::Buffer stagingBuffer = device.createBuffer(vk::BufferCreateInfo {
		.size = buffer.size,
		.usage = vk::BufferUsageFlagBits::eTransferSrc,
	});

	SubAllocation stagingAllocation = m_stagingAllocation.subAllocate(
		device.getBufferMemoryRequirements(stagingBuffer)
	);

	device.bindBufferMemory(
		stagingBuffer,
		m_stagingAllocation.getAllocation().memory,
		stagingAllocation.offset
	);

	updateFunction(
		static_cast<std::byte*>(m_stagingAllocation.getAllocation().address) +
		stagingAllocation.offset
	);

	std::vector<ResourceManager::BufferCopy> info = {
			{
				.origin = stagingBuffer,
				.destination = m_buffers[handle.value].buffer,
				.copy = {
					 .size = buffer.size,
				 },
			}
		};

	copyBuffers(info);
	device.destroyBuffer(stagingBuffer);
}
