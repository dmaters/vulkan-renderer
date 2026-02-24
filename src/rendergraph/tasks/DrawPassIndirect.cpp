#include "DrawPassIndirect.hpp"

#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "scene/Scene.hpp"

void DrawPassIndirect(
	TaskContext& context,
	MaterialIndex materialIndex,
	const std::vector<uint32_t>& primitives
) {
	assert(
		context.inputs.back().second == ResourceUsage::Type::IndirectBufferRead
	);
	assert(context.outputs.back().second == ResourceUsage::Type::Undefined);

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

	auto indirectBufferOutput =
		context.getOutputSpan<vk::DrawIndexedIndirectCommand>(
			context.outputs.size() - 1, true
		);

	auto indirectMaterialBufferOutput =
		context.getOutputSpan<uint32_t>(context.outputs.size() - 2, true);

	for (int i = 0; i < primitives.size(); i++) {
		uint32_t primitiveIndex = primitives[i];
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		indirectBufferOutput[i] = vk::DrawIndexedIndirectCommand {
			.indexCount = primitive.indexCount,
			.instanceCount = 1,
			.firstIndex = primitive.baseIndex,
			.vertexOffset = static_cast<int32_t>(primitive.baseVertex),
			.firstInstance = 0,
		};

		indirectMaterialBufferOutput[i] = primitiveIndex;
	}

	Buffer& indirectBuffer =
		context.getInput<Buffer&>(context.inputs.size() - 1);

	context.commandBuffer.drawIndexedIndirect(
		indirectBuffer.buffer,
		0,
		context.currentFrame > 3 ? primitives.size()
								 : 0,  // Warmup for syncronization
		sizeof(vk::DrawIndexedIndirectCommand)
	);
}
