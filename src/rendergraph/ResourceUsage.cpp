#include "ResourceUsage.hpp"

std::optional<vk::DescriptorType> ResourceUsage::getDescriptorType(
	ResourceUsage::Type usage, bool isBuffer
) {
	switch (usage) {
		case ResourceUsage::Type::SampledRead:
			return vk::DescriptorType::eSampledImage;
		case ResourceUsage::Type::ShaderWrite:
			return isBuffer ? vk::DescriptorType::eStorageBuffer
			                : vk::DescriptorType::eStorageImage;
		case ResourceUsage::Type::ShaderRead:
			return isBuffer ? vk::DescriptorType::eStorageBuffer
			                : vk::DescriptorType::eSampledImage;
		case ResourceUsage::Type::StorageBufferWrite:
		case ResourceUsage::Type::StorageBufferRead:
			return vk::DescriptorType::eStorageBuffer;
		case ResourceUsage::Type::UniformBuffer:
			return vk::DescriptorType::eUniformBuffer;
		default:
			return std::nullopt;
	}
}
