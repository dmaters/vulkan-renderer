#include "Allocation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <iostream>
#include <list>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"

Allocation::Allocation(
	vk::MemoryPropertyFlags requiredType, uint32_t requiredSize
) {
	auto& physicalDevice = Instance::Get().physicalDevice;
	auto& device = Instance::Get().device;

	auto properties = physicalDevice.getMemoryProperties();

	for (int i = 0; i < properties.memoryTypeCount; i++) {
		if ((properties.memoryTypes[i].propertyFlags & requiredType) !=
		    requiredType)
			continue;
		memory = device.allocateMemory(vk::MemoryAllocateInfo {
			.allocationSize = requiredSize,
			.memoryTypeIndex = (uint32_t)i,
		});

		if (requiredType & vk::MemoryPropertyFlagBits::eHostCoherent)
			address = device.mapMemory(memory, 0, requiredSize);

		size = requiredSize;

		return;
	}
}
