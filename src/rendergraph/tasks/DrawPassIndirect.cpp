#include "DrawPassIndirect.hpp"

#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "scene/Scene.hpp"

void DrawPassIndirect(
	TaskContext& context,
	MaterialIndex materialIndex,
	const std::vector<uint32_t>& primitives,
	uint32_t indirectBufferIndex,
	uint32_t primitiveBufferIndex,
	uint32_t pushConstant
) {
	const Pipeline& material =
		context.materialManager.getPipeline(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, material.pipeline
	);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		material.pipelineLayout,
		1,
		{ context.materialManager.getTextureSet() },
		{}
	);

	TaskContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eGraphics,
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

	auto indirectBufferValues =
		context.getInputSpan<vk::DrawIndexedIndirectCommand>(
			indirectBufferIndex
		);

	auto primitiveBufferValues =
		context.getInputSpan<uint32_t>(primitiveBufferIndex);

	context.commandBuffer.pushConstants(
		material.pipelineLayout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(uint32_t),
		&pushConstant
	);

	for (int i = 0; i < primitives.size(); i++) {
		uint32_t primitiveIndex = primitives[i];
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		indirectBufferValues[i] = vk::DrawIndexedIndirectCommand {
			.indexCount = primitive.indexCount,
			.instanceCount = 1,
			.firstIndex = primitive.baseIndex,
			.vertexOffset = static_cast<int32_t>(primitive.baseVertex),
			.firstInstance = 0,
		};

		primitiveBufferValues[i] = primitiveIndex;
	}

	Buffer& indirectBuffer = context.getInput<Buffer&>(indirectBufferIndex);

	context.commandBuffer.drawIndexedIndirect(
		indirectBuffer.buffer,
		(indirectBuffer.size / 3) * (context.currentFrame % 3),
		primitives.size(),
		sizeof(vk::DrawIndexedIndirectCommand)
	);
}
