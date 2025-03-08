#include "MemoryAllocator.hpp"

#include <cassert>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"

MemoryAllocator::MemoryAllocator(Instance& instance) : m_instance(instance) {
	vk::PhysicalDeviceMemoryProperties properties =
		m_instance.physicalDevice.getMemoryProperties();

	for (int i = 0; i < properties.memoryTypeCount; i++) {
		vk::MemoryType type = properties.memoryTypes[i];
		if (type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal &&
		    !(type.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
			m_memoryType[Location::DeviceLocal] = 1;
		else if (type.propertyFlags &
		             vk::MemoryPropertyFlagBits::eHostCoherent &&
		         !(type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal
		         ))
			m_memoryType[Location::HostMapped] = i;
	}

	assert(m_memoryType.size() == 2);

	allocate(m_localAllocation, Location::HostMapped);
}

bool createSubAllocation(
	MemoryAllocator::Allocation& allocation,
	vk::MemoryRequirements requirements,
	MemoryAllocator::SubAllocation& subAllocation
) {
	uint32_t startingOffset = 0;
	if (allocation.occupiedOffset > 0) {
		startingOffset = allocation.occupiedOffset +
		                 (requirements.alignment -
		                  allocation.occupiedOffset % requirements.alignment);
	}

	if ((256ull << 20) - startingOffset < requirements.size) return false;

	subAllocation = { .offset = startingOffset, .size = requirements.size };

	allocation.occupiedOffset = startingOffset + requirements.size;
	return true;
}

MemoryAllocator::SubAllocation MemoryAllocator::subAllocate(
	vk::MemoryRequirements requirements, Location location
) {
	SubAllocation subAllocation;
	if (location == Location::HostMapped &&
	    createSubAllocation(m_localAllocation, requirements, subAllocation)) {
		subAllocation.allocationIndex = 0;
		return subAllocation;
	}

	for (int i = 0; i < m_deviceAllocations.size(); i++) {
		if (!createSubAllocation(
				m_deviceAllocations[i], requirements, subAllocation
			))
			continue;

		subAllocation.allocationIndex = i + 1;
		return subAllocation;
	}

	Allocation allocation;
	assert(allocate(allocation, Location::DeviceLocal));
	assert(createSubAllocation(allocation, requirements, subAllocation));
	subAllocation.allocationIndex = m_deviceAllocations.size() + 1;
	m_deviceAllocations.push_back(allocation);
	return subAllocation;
}

MemoryAllocator::SubAllocation MemoryAllocator::allocate(
	vk::Buffer buffer, Location location
) {
	vk::MemoryRequirements requirements =
		m_instance.device.getBufferMemoryRequirements(buffer);

	SubAllocation subAllocation = subAllocate(requirements, location);

	Allocation& allocation =
		subAllocation.allocationIndex == 0
			? m_localAllocation
			: m_deviceAllocations[subAllocation.allocationIndex - 1];

	m_instance.device.bindBufferMemory(
		buffer, allocation.memory, subAllocation.offset
	);
	if (location == Location::HostMapped)
		subAllocation.address =
			(char*)allocation.address + subAllocation.offset;
	return subAllocation;
}

MemoryAllocator::SubAllocation MemoryAllocator::allocate(
	vk::Image image, Location location
) {
	vk::MemoryRequirements requirements =
		m_instance.device.getImageMemoryRequirements(image);
	SubAllocation subAllocation = subAllocate(requirements, location);

	Allocation& allocation =
		subAllocation.allocationIndex == 0
			? m_localAllocation
			: m_deviceAllocations[subAllocation.allocationIndex - 1];

	m_instance.device.bindImageMemory(
		image, allocation.memory, vk::DeviceSize { subAllocation.offset }
	);

	return subAllocation;
}

bool MemoryAllocator::allocate(Allocation& allocation, Location location) {
	vk::MemoryAllocateInfo info {
		.allocationSize = 256ull << 20,
		.memoryTypeIndex = m_memoryType[location],
	};

	allocation = {
		.memory = m_instance.device.allocateMemory(info),
		.occupiedOffset = 0,
	};
	if (location == Location::HostMapped) {
		allocation.address =
			m_instance.device.mapMemory(allocation.memory, 0, 256ull << 20);
	}
	return true;
}