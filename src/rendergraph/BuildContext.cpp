#include "BuildContext.hpp"

#include "rendergraph/ResourceUsage.hpp"
#include "vulkan/vulkan.hpp"

bool isValidImageDescriptor(ResourceUsage::Type usage) {
	switch (usage) {
		case ResourceUsage::Type::SampledRead:
		case ResourceUsage::Type::ShaderWrite:
		case ResourceUsage::Type::ShaderRead:
			return true;
		default:
			return false;
	}
}

bool isValidBufferDescriptor(ResourceUsage::Type usage) {
	switch (usage) {
		case ResourceUsage::Type::StorageBufferRead:
		case ResourceUsage::Type::StorageBufferWrite:
		case ResourceUsage::Type::UniformBuffer:
			return true;
		default:
			return false;
	}
}

Task::BuildContext::Descriptors Task::BuildContext::getDescriptors() const {
	uint32_t bindingCount = 0;
	Task::BuildContext::Descriptors resources;

	uint32_t maxPossibleSize = inputs.size() + outputs.size();
	resources._imageInfo.reserve(maxPossibleSize);
	resources._bufferInfo.reserve(maxPossibleSize);

	for (auto [index, usage] : inputs) {
		if (!isValidImageDescriptor(usage) && !isValidBufferDescriptor(usage))
			continue;

		if (rendergraph::internal::ResourceIndexer::ResourceIndex_getType(
				index
			) == rendergraph::internal::ResourceIndexer::ResourceType::Image) {
			Image& image = resourceManager.getImage(images[index]);

			resources._imageInfo.push_back(
				{
					.imageView = image.view,
					.imageLayout = ResourceUsage::GetAccess(usage).layout,
				}
			);

			resources.descriptors.push_back(
				vk::WriteDescriptorSet {
					.dstSet = nullptr,
					.dstBinding = bindingCount,
					.descriptorCount = 1,
					.descriptorType =
						ResourceUsage::GetDescriptorType(usage, false).value(),
					.pImageInfo = &resources._imageInfo.back(),
				}
			);
		} else {
			Buffer& buffer = resourceManager.getBuffer(buffers[index]);

			resources._bufferInfo.push_back(
				{
					.buffer = buffer.buffer,
					.range = buffer.size,
				}
			);

			resources.descriptors.push_back(
				vk::WriteDescriptorSet {
					.dstSet = nullptr,
					.dstBinding = bindingCount,
					.descriptorCount = 1,
					.descriptorType =
						ResourceUsage::GetDescriptorType(usage, true).value(),
					.pBufferInfo = &resources._bufferInfo.back(),
				}
			);
		}
		bindingCount++;
	}

	for (auto [index, usage] : outputs) {
		if (!isValidImageDescriptor(usage) && !isValidBufferDescriptor(usage))
			continue;

		if (rendergraph::internal::ResourceIndexer::ResourceIndex_getType(
				index
			) == rendergraph::internal::ResourceIndexer::ResourceType::Image) {
			Image& image = resourceManager.getImage(images[index]);

			resources._imageInfo.push_back(
				{
					//	.sampler = sampler,
					.imageView = image.view,
					.imageLayout = ResourceUsage::GetAccess(usage).layout,
				}
			);

			resources.descriptors.push_back(
				vk::WriteDescriptorSet {
					.dstSet = nullptr,
					.dstBinding = bindingCount,
					.descriptorCount = 1,
					.descriptorType =
						ResourceUsage::GetDescriptorType(usage, false).value(),
					.pImageInfo = &resources._imageInfo.back(),
				}
			);
		} else {
			Buffer& buffer = resourceManager.getBuffer(buffers[index]);

			resources._bufferInfo.push_back(
				{
					.buffer = buffer.buffer,
					.range = buffer.size,
				}
			);

			resources.descriptors.push_back(
				vk::WriteDescriptorSet {
					.dstSet = nullptr,
					.dstBinding = bindingCount,
					.descriptorCount = 1,
					.descriptorType =
						ResourceUsage::GetDescriptorType(usage, true).value(),
					.pBufferInfo = &resources._bufferInfo.back(),
				}
			);
		}
		bindingCount++;
	}

	return resources;
}
