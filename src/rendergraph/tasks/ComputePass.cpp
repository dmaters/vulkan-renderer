#include "ComputePass.hpp"

#include <vulkan/vulkan.hpp>

#include "TaskContext.hpp"
#include "material/Pipeline.hpp"
#include "rendergraph/tasks/Task.hpp"

void ComputePass(
	TaskContext& context, MaterialIndex materialIndex, glm::uvec3 dispatchSize
) {
	vk::CommandBuffer& commandBuffer = context.commandBuffer;

	const Pipeline& material =
		context.materialManager.getPipeline(materialIndex);

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

	TaskContext::Descriptors descriptors = context.getDescriptors();

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
