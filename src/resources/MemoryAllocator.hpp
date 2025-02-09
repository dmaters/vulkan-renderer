#pragma once

#include <vk_mem_alloc.h>

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "MemoryAllocator.hpp"

class MemoryAllocator {
public:
	enum class Location {
		DeviceLocal,
		HostMapped,
	};
	struct Allocation;
	struct BufferAllocation;
	struct ImageAllocation;

private:
	Instance& m_instance;
	VmaAllocator m_allocator;
	vk::Queue m_queue;
	vk::CommandPool m_commandPool;

	vk::Fence m_uploadFinishedFence;

public:
	MemoryAllocator(Instance& instance);
	BufferAllocation allocateBuffer(
		const vk::BufferCreateInfo& bufferInfo, Location location
	);
	BufferAllocation allocateStagingBuffer(size_t size);

	ImageAllocation allocateImage(const vk::ImageCreateInfo& image);

	void freeAllocation(Allocation& allocation);

	// Buffer getIndexBuffer();
	void copyBuffer(
		BufferAllocation& origin,
		BufferAllocation& destination,
		vk::BufferCopy offset
	);
	void copyToImage(
		BufferAllocation& origin,
		ImageAllocation& destination,
		vk::BufferImageCopy offset
	);
};

struct MemoryAllocator::Allocation {
	VmaAllocation allocation;
	MemoryAllocator& allocator;
};

struct MemoryAllocator::BufferAllocation {
	Allocation base;
	vk::Buffer buffer;
	void updateData(
		const std::vector<unsigned char>& data, unsigned int offset = 0
	);
};

struct MemoryAllocator::ImageAllocation {
	Allocation base;
	vk::Image image;
};
