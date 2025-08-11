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

MemoryAllocator::MemoryAllocator() {
	Instance& instance = Instance::Get();

	vk::PhysicalDeviceMemoryProperties properties =
		instance.physicalDevice.getMemoryProperties();

	for (int i = properties.memoryTypeCount-1; i >= 0; i--) {
		vk::MemoryType type = properties.memoryTypes.at(i);
		if (properties.memoryHeaps.at(type.heapIndex).size < (1024ull << 20))
			continue;

		if ((type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) &&
		    !(type.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
			m_memoryType[AllocationLocation::Device] = i;
		else if ((type.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible
		         ) &&
		         (type.propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent
		         ))
			m_memoryType[AllocationLocation::Host] = i;
	}

	assert(m_memoryType.size() == 2);
	Allocation allocation;
	allocate(allocation, AllocationType::Persistent, AllocationLocation::Host);
	m_localPersistent = std::make_unique<BuddyAllocator>(allocation);
	allocate(allocation, AllocationType::Staging, AllocationLocation::Host);
	m_stagingAllocation = std::make_unique<RingAllocation>(allocation);
	allocate(
		allocation, AllocationType::Persistent, AllocationLocation::Device
	);
	m_devicePersistent = std::make_unique<BuddyAllocator>(allocation);
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
		bool res = m_devicePersistent->subAllocate(subAllocation, requirements);
		subAllocation.allocationIndex = 2;
		return res;
	}

	if (type == AllocationType::Persistent &&
	    location == AllocationLocation::Host) {
		memory = m_localPersistent->getMemory();
		bool res = m_localPersistent->subAllocate(subAllocation, requirements);
		subAllocation.allocationIndex = 1;
		return res;
	}

	memory = m_stagingAllocation->getMemory();
	bool res = m_stagingAllocation->subAllocate(subAllocation, requirements);
	subAllocation.allocationIndex = 0;
	return res;
}
SubAllocation MemoryAllocator::allocate(
	vk::Buffer buffer, AllocationType type, AllocationLocation location
) {
	vk::Device& device = Instance::Get().device;
	vk::MemoryRequirements requirements =
		device.getBufferMemoryRequirements(buffer);
	SubAllocation subAllocation;
	vk::DeviceMemory memory;
	getSubAllocation(subAllocation, memory, requirements, type, location);
	device.bindBufferMemory(buffer, memory, subAllocation.offset);
	return subAllocation;
}

SubAllocation MemoryAllocator::allocate(
	vk::Image image, AllocationType type, AllocationLocation location
) {
	vk::Device& device = Instance::Get().device;
	vk::MemoryRequirements requirements =
		device.getImageMemoryRequirements(image);
	SubAllocation subAllocation;

	vk::DeviceMemory memory;
	getSubAllocation(subAllocation, memory, requirements, type, location);
	device.bindImageMemory(
		image, memory, vk::DeviceSize { subAllocation.offset }
	);

	return subAllocation;
}

bool MemoryAllocator::allocate(
	Allocation& allocation, AllocationType type, AllocationLocation location
) {
	vk::Device& device = Instance::Get().device;

	vk::MemoryAllocateInfo info {
		.allocationSize = 1024ull << 20,
		.memoryTypeIndex = m_memoryType[location],
	};

	allocation = {
		.memory = device.allocateMemory(info),
	};
	if (location == AllocationLocation::Host) {
		allocation.address =
			device.mapMemory(allocation.memory, 0, 256ull << 20);
	}
	return true;
}

void MemoryAllocator::free(SubAllocation subAllocation) {
	switch (subAllocation.allocationIndex) {
		case 0:
			m_stagingAllocation->free(subAllocation);
			break;
		case 1:
			m_localPersistent->free(subAllocation);
			break;
		case 2:
			m_devicePersistent->free(subAllocation);
			break;
	}
}
