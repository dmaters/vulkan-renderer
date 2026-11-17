#include "Allocation.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"

Allocation::Allocation(vk::MemoryPropertyFlags requiredType, uint32_t requiredSize) {
	auto& physicalDevice = Instance::Get().physicalDevice;
	auto& device = Instance::Get().device;

	auto properties = physicalDevice.getMemoryProperties();

	for (int i = 0; i < properties.memoryTypeCount; i++) {
		auto& memoryType = properties.memoryTypes[i];
		if ((memoryType.propertyFlags & requiredType) != requiredType ||
			(properties.memoryHeaps[memoryType.heapIndex].size < requiredSize))
			continue;
		vk::MemoryAllocateFlagsInfo flagsInfo { .flags = vk::MemoryAllocateFlagBits::eDeviceAddress };

		auto [allocateStatus, allocatedMemory] = device.allocateMemory(
			{
				.pNext = &flagsInfo,
				.allocationSize = requiredSize,
				.memoryTypeIndex = (uint32_t)i,
			}
		);
		memory = allocatedMemory;

		if (requiredType & vk::MemoryPropertyFlagBits::eHostVisible) {
			auto [mapStatus, createdAddress] = device.mapMemory(memory, 0, requiredSize);
			address = static_cast<std::byte*>(createdAddress);
		};

		size = requiredSize;

		return;
	}
}

void Allocation::free() {
	vk::Device& device = Instance::Get().device;
	if (address) device.unmapMemory(memory);
	device.freeMemory(memory);
}
