#include "ComputePass.hpp"

#include <vulkan/vulkan_handles.hpp>

struct TransientResources {
	std::vector<vk::DescriptorImageInfo> _imageInfo = {};
	std::vector<vk::DescriptorBufferInfo> _bufferInfo = {};
	std::vector<vk::WriteDescriptorSet> descriptors = {};
};

TransientResources getTransientResources(
	ResourceManager& resourceManager,
	std::vector<ResourceIndex>& inputs,
	std::vector<ResourceIndex>& outputs,
	std::unordered_map<ResourceIndex, ImageHandle>& images,
	std::unordered_map<ResourceIndex, BufferHandle>& buffers,
	vk::Sampler sampler
) {
	uint32_t bindingCount = 0;
	TransientResources resources;

	resources._imageInfo.reserve(inputs.size() + outputs.size());

	resources._bufferInfo.reserve(inputs.size());
	for (auto index : inputs) {
		if (images.contains(index)) {
			Image& image = resourceManager.getImage(images[index]);

			resources._imageInfo.push_back({
				.sampler = sampler,
				.imageView = image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			});

			resources.descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &resources._imageInfo.back(),
			});
		} else if (buffers.contains(index)) {
			Buffer& buffer = resourceManager.getBuffer(buffers[index]);

			resources._bufferInfo.push_back({
				.buffer = buffer.buffer,
				.range = buffer.size,
			});

			resources.descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.pBufferInfo = &resources._bufferInfo.back(),
			});
		}
		bindingCount++;
	}

	for (auto index : outputs) {
		if (images.contains(index)) {
			Image& image = resourceManager.getImage(images[index]);

			resources._imageInfo.push_back({
				//	.sampler = sampler,
				.imageView = image.view,
				.imageLayout = vk::ImageLayout::eGeneral,
			});

			resources.descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageImage,
				.pImageInfo = &resources._imageInfo.back(),
			});
		} else if (buffers.contains(index)) {
			Buffer& buffer = resourceManager.getBuffer(buffers[index]);

			resources._bufferInfo.push_back({
				.buffer = buffer.buffer,
				.range = buffer.size,
			});

			resources.descriptors.push_back(vk::WriteDescriptorSet {
				.dstSet = nullptr,
				.dstBinding = bindingCount,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.pBufferInfo = &resources._bufferInfo.back(),
			});
		}
		bindingCount++;
	}

	return resources;
}
void ComputePass(
	TaskContext& context, MaterialIndex materialIndex, glm::uvec3 dispatchSize
) {
	vk::CommandBuffer& commandBuffer = context.commandBuffer;

	const Material& material =
		context.materialManager.getMaterial(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eCompute, material.pipeline.pipeline
	);

	vk::DescriptorSet globalSet = context.materialManager.getGlobalSet();

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		material.pipeline.pipelineLayout,
		0,
		{ globalSet, material.materialSet },
		{}
	);

	TransientResources transientResources = getTransientResources(
		context.resourceManager,
		context.inputs,
		context.outputs,
		context.images,
		context.buffers,
		context.materialManager.getLinearSampler()
	);

	if (transientResources.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSetKHR(
			vk::PipelineBindPoint::eCompute,
			material.pipeline.pipelineLayout,
			2,
			transientResources.descriptors
		);
	}

	commandBuffer.dispatch(dispatchSize.x, dispatchSize.y, dispatchSize.z);
}