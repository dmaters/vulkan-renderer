#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Allocation.hpp"
#include "Instance.hpp"

enum class AllocationLocation {
	Device,
	Host,
};
enum class AllocationType {
	Persistent,
	Transient,
	Staging,
};

class MemoryAllocator {
public:
private:
	Instance& m_instance;
	vk::Queue m_queue;
	vk::CommandPool m_commandPool;

	std::unique_ptr<RingAllocation> m_stagingAllocation;
	std::unique_ptr<RingAllocation> m_localPersistent;
	std::unique_ptr<RingAllocation> m_devicePersistent;

	bool allocate(
		Allocation& allocation, AllocationType type, AllocationLocation location
	);
	bool getSubAllocation(
		SubAllocation& suballocation,
		vk::DeviceMemory& memory,
		vk::MemoryRequirements requirements,
		AllocationType type,
		AllocationLocation location
	);
	std::map<AllocationLocation, uint32_t> m_memoryType;

public:
	MemoryAllocator(Instance& instance);
	SubAllocation allocate(
		vk::Buffer buffer, AllocationType type, AllocationLocation location
	);
	SubAllocation allocate(
		vk::Image image, AllocationType type, AllocationLocation location
	);

	void getMemoryTypes();
	void updateData(SubAllocation& allocation, std::vector<std::byte> data);
};
