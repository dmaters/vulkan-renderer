#include "DrawPass.hpp"

#include "material/MaterialManager.hpp"
#include "material/Pipeline.hpp"
#include "scene/Scene.hpp"

void DrawPass::Quad(Task::BuildContext& context, MaterialIndex materialIndex) {
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

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

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
	context.commandBuffer.draw(3, 1, 0, 0);
}

void DrawPass::Indirect(
	Task::BuildContext& context,
	MaterialIndex material,
	const std::vector<uint32_t>& primitives,
	uint32_t pushConstant,
	rendergraph::ResourceIndex indirectBufferLocalIndex,
	rendergraph::ResourceIndex primitiveBufferLocalIndex
) {
	Buffer& indirectBufferLocal = context.getBuffer(indirectBufferLocalIndex);
	Buffer& primitiveMapLocal = context.getBuffer(primitiveBufferLocalIndex);
	Buffer& indirectBufferDevice = context.getInput<Buffer&>(0);
	Buffer& primitiveMapDevice = context.getInput<Buffer&>(1);

	vk::BufferCopy2 primitiveCopy {
		.size = primitiveMapLocal.size,
	};
	context.commandBuffer.copyBuffer2(
		{
			.srcBuffer = primitiveMapLocal.buffer,
			.dstBuffer = primitiveMapDevice.buffer,
			.regionCount = 1,
			.pRegions = &primitiveCopy,
		}
	);

	std::array<vk::BufferMemoryBarrier2, 2> barriers = {
		vk::BufferMemoryBarrier2 {
								  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								  .dstStageMask = vk::PipelineStageFlagBits2::eDrawIndirect,
								  .dstAccessMask = vk::AccessFlagBits2::eIndirectCommandRead,
								  .buffer = indirectBufferDevice.buffer,
								  .size = indirectBufferDevice.size,
								  },
		vk::BufferMemoryBarrier2 {
								  .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
								  .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
								  .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
								  .dstAccessMask = vk::AccessFlagBits2::eShaderStorageRead,
								  .buffer = primitiveMapDevice.buffer,
								  .size = primitiveMapDevice.size,
								  },
	};

	context.commandBuffer.pipelineBarrier2(
		{
			.bufferMemoryBarrierCount = 2,
			.pBufferMemoryBarriers = barriers.data(),
		}
	);

	const Pipeline& pipeline = context.materialManager.getPipeline(material);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, pipeline.pipeline
	);

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline.pipelineLayout,
		1,
		{ context.materialManager.getTextureSet() },
		{}
	);

	Task::BuildContext::Descriptors descriptors = context.getDescriptors();

	if (descriptors.descriptors.size() > 0) {
		context.commandBuffer.pushDescriptorSet(
			vk::PipelineBindPoint::eGraphics,
			pipeline.pipelineLayout,
			0,
			descriptors.descriptors
		);
	} else
		context.commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			pipeline.pipelineLayout,
			0,
			{ context.materialManager.getEmptySet() },
			{}
		);

	auto indirectBufferValues =
		(vk::DrawIndexedIndirectCommand*)indirectBufferLocal.data;

	auto primitiveBufferValues = (uint32_t*)primitiveMapLocal.data;

	context.commandBuffer.pushConstants(
		pipeline.pipelineLayout,
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

	vk::BufferCopy2 indirectCopy {
		.size = indirectBufferLocal.size,
	};
	context.commandBuffer.copyBuffer2(
		{
			.srcBuffer = indirectBufferLocal.buffer,
			.dstBuffer = indirectBufferDevice.buffer,
			.regionCount = 1,
			.pRegions = &indirectCopy,
		}
	);

	context.commandBuffer.drawIndexedIndirect(
		indirectBufferDevice.buffer,
		indirectBufferDevice.size,
		primitives.size(),
		sizeof(vk::DrawIndexedIndirectCommand)
	);
}
