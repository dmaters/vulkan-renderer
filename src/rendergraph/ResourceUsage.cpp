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
		case ResourceUsage::Type::ColorAttachmentWrite:
		case ResourceUsage::Type::DepthStencilRead:
		case ResourceUsage::Type::DepthStencilWrite:
		case ResourceUsage::Type::VertexBuffer:
		case ResourceUsage::Type::IndexBuffer:
		case ResourceUsage::Type::TransferSrc:
		case ResourceUsage::Type::TransferDst:
			return std::nullopt;
	}
}
