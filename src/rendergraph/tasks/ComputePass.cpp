#include "ComputePass.hpp"

#include <vulkan/vulkan.hpp>

#include "../BuildContext.hpp"
#include "../Task.hpp"
#include "material/Pipeline.hpp"

void ComputePass(
	Task::BuildContext& context,
	MaterialIndex materialIndex,
	glm::uvec3 dispatchSize
) {
	vk::CommandBuffer& commandBuffer = context.commandBuffer;

	Pipeline material = context.materialManager.getPipeline(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eCompute, material.pipeline
	);

	vk::DescriptorSet textureSet = context.materialManager.getTextureSet();

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eCompute,
		material.pipelineLayout,
		1,
		{ textureSet },
		{}
	);

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSetKHR(
			vk::PipelineBindPoint::eCompute,
			material.pipelineLayout,
			0,
			descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			material.pipelineLayout,
			0,
			{ context.materialManager.getEmptySet() },
			{}
		);

	commandBuffer.dispatch(dispatchSize.x, dispatchSize.y, dispatchSize.z);
}
