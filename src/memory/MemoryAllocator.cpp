#include "MemoryAllocator.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Allocation.hpp"
#include "Instance.hpp"

MemoryAllocator::MemoryAllocator(Instance& instance) : m_instance(instance) {
	vk::PhysicalDeviceMemoryProperties properties =
		m_instance.physicalDevice.getMemoryProperties();

	for (int i = properties.memoryTypeCount; i >= 0; i--) {
		vk::MemoryType type = properties.memoryTypes[i];
		if (properties.memoryHeaps[type.heapIndex].size < 256ull << 20)
			continue;

		if (type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
			m_memoryType[AllocationLocation::Device] = i;
		else if (type.propertyFlags &
		             vk::MemoryPropertyFlagBits::eHostCoherent &&
		         (type.propertyFlags & vk::MemoryPropertyFlagBits::eHostCached))
			m_memoryType[AllocationLocation::Host] = i;
	}

	assert(m_memoryType.size() == 2);
	Allocation allocation;
	allocate(allocation, AllocationType::Persistent, AllocationLocation::Host);
	m_localPersistent = std::make_unique<RingAllocation>(allocation);
	allocate(allocation, AllocationType::Staging, AllocationLocation::Host);
	m_stagingAllocation = std::make_unique<RingAllocation>(allocation);
	allocate(
		allocation, AllocationType::Persistent, AllocationLocation::Device
	);
	m_devicePersistent = std::make_unique<RingAllocation>(allocation);
}
bool MemoryAllocator::getSubAllocation(
	SubAllocation& subAllocation,
	vk::DeviceMemory& memory,
	vk::MemoryRequirements requirements,
	AllocationType type,
	AllocationLocation location
) {
	if (type == AllocationType::Persistent &&
	    location == AllocationLocation::Device) {
		memory = m_devicePersistent->getMemory();
		return m_devicePersistent->subAllocate(subAllocation, requirements);
	}

	if (type == AllocationType::Persistent &&
	    location == AllocationLocation::Host) {
		memory = m_localPersistent->getMemory();
		return m_localPersistent->subAllocate(subAllocation, requirements);
	}

	memory = m_stagingAllocation->getMemory();
	return m_stagingAllocation->subAllocate(subAllocation, requirements);
}
SubAllocation MemoryAllocator::allocate(
	vk::Buffer buffer, AllocationType type, AllocationLocation location
) {
	vk::MemoryRequirements requirements =
		m_instance.device.getBufferMemoryRequirements(buffer);
	SubAllocation subAllocation;
	vk::DeviceMemory memory;
	getSubAllocation(subAllocation, memory, requirements, type, location);
	m_instance.device.bindBufferMemory(buffer, memory, subAllocation.offset);
	return subAllocation;
}

SubAllocation MemoryAllocator::allocate(
	vk::Image image, AllocationType type, AllocationLocation location
) {
	vk::MemoryRequirements requirements =
		m_instance.device.getImageMemoryRequirements(image);
	SubAllocation subAllocation;

	vk::DeviceMemory memory;
	getSubAllocation(subAllocation, memory, requirements, type, location);
	m_instance.device.bindImageMemory(
		image, memory, vk::DeviceSize { subAllocation.offset }
	);

	return subAllocation;
}

bool MemoryAllocator::allocate(
	Allocation& allocation, AllocationType type, AllocationLocation location
) {
	vk::MemoryAllocateInfo info {
		.allocationSize = 256ull << 20,
		.memoryTypeIndex = m_memoryType[location],
	};

	allocation = {
		.memory = m_instance.device.allocateMemory(info),
	};
	if (location == AllocationLocation::Host) {
		allocation.address =
			m_instance.device.mapMemory(allocation.memory, 0, 256ull << 20);
	}
	return true;
}