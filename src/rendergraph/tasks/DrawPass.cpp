#include "DrawPass.hpp"

#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "scene/Scene.hpp"

void DrawPass(
	TaskContext& context,
	MaterialIndex materialIndex,
	const std::vector<uint32_t>& primitives
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

	for (const uint32_t primitiveIndex : primitives) {
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		context.commandBuffer.pushConstants(
			material.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex |
				vk::ShaderStageFlagBits::eFragment,
			0,
			sizeof(uint32_t),
			&primitiveIndex  // TODO: better indexing for material instances
		);

		context.commandBuffer.drawIndexed(
			primitive.indexCount,
			1,
			primitive.baseIndex,
			primitive.baseVertex,
			0
		);
	}
}
