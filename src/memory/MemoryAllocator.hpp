#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"

class MemoryAllocator {
public:
	enum class Location {
		DeviceLocal,
		HostMapped,
	};

	struct Allocation {
		vk::DeviceMemory memory;
		uint64_t occupiedOffset;
		void* address = nullptr;
	};

	struct AllocationInfo;
	struct SubAllocation;

private:
	Instance& m_instance;
	vk::Queue m_queue;
	vk::CommandPool m_commandPool;

	Allocation m_localAllocation;
	std::vector<Allocation> m_deviceAllocations;

	bool allocate(Allocation& allocation, Location location);
	SubAllocation subAllocate(
		vk::MemoryRequirements requirements, Location location
	);
	std::map<Location, uint32_t> m_memoryType;

public:
	MemoryAllocator(Instance& instance);
	SubAllocation allocate(
		vk::Buffer buffer, Location location = Location::DeviceLocal
	);
	SubAllocation allocate(
		vk::Image image, Location location = Location::DeviceLocal
	);

	void getMemoryTypes();
	void updateData(SubAllocation& allocation, std::vector<std::byte> data);
};
struct MemoryAllocator::SubAllocation {
	uint8_t allocationIndex;
	uint64_t offset;
	size_t size;
	void* address = nullptr;
};