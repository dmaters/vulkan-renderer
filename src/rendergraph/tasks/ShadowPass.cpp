#include "ShadowPass.hpp"


#include <vulkan/vulkan.hpp>

constexpr int32_t CASCADE_SIZE = 1024;


void ShadowPass(
	TaskContext& context, uint8_t cascade, std::vector<Primitive>& primitives
) {
	MaterialIndex materialIndex =
		context.materialManager.getMaterialIndex("shadow_map");
	const Material& material =
		context.materialManager.getMaterial(materialIndex);

	context.commandBuffer.bindPipeline(
		vk::PipelineBindPoint::eGraphics, material.pipeline.pipeline
	);

	vk::DescriptorSet globalSet = context.materialManager.getGlobalSet();

	context.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		material.pipeline.pipelineLayout,
		0,
		{ globalSet, material.materialSet },
		{}
	);
	ImageHandle handle = context.images[context.outputs[0]];
	Image& image = context.resourceManager.getImage(handle);

	vk::RenderingAttachmentInfo attachment {

		.imageView = image.view,
		.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = { .depthStencil = { .depth = 1, .stencil = 0 }, },
	
	};

	vk::Rect2D renderArea =  {
		{
			CASCADE_SIZE * cascade,
			0,
		},
		{
			CASCADE_SIZE,
			CASCADE_SIZE
		}

	};

	context.commandBuffer.beginRendering(vk::RenderingInfo{
		.layerCount = 1,
		.renderArea = renderArea,
		.pDepthAttachment = &attachment,
		
	});
	context.commandBuffer.setScissor(0,{renderArea});
	context.commandBuffer.setViewport(0,{{
		.x = (float)renderArea.offset.x,
		.width = CASCADE_SIZE,
		.height = CASCADE_SIZE,
	}});
	uint32_t pcascade = cascade;
	context.commandBuffer.pushConstants(
		material.pipeline.pipelineLayout,
		vk::ShaderStageFlagBits::eVertex |
			vk::ShaderStageFlagBits::eFragment,
		0,
		sizeof(uint32_t),
		&pcascade
	);

	Buffer& positionBuffer =
		context.resourceManager.getNamedBuffer("vertex_buffer_positions");
	Buffer& attributeBuffer =
		context.resourceManager.getNamedBuffer("vertex_buffer_attributes");
	Buffer& instanceBuffer = context.resourceManager.getNamedBuffer("instance_buffer");
	Buffer& indexBuffer = context.resourceManager.getNamedBuffer("index_buffer");

	context.commandBuffer.bindVertexBuffers(
		0,
		{
			positionBuffer.buffer,
			attributeBuffer.buffer,
			instanceBuffer.buffer,
		},
		{ 0, 0, 0 }
	);
	context.commandBuffer.bindIndexBuffer(
		indexBuffer.buffer, 0, vk::IndexType::eUint32
	);


	for (const Primitive& primitive : primitives) {
		if (!std::any_of(
				primitive.materials.begin(),
				primitive.materials.end(),
				[&passMaterial =
		             materialIndex](MaterialInstance primitiveMaterial) {
					return primitiveMaterial.index == passMaterial;
				}
			))
			continue;


		context.commandBuffer.drawIndexed(
			primitive.indexCount,
			primitive.instanceCount,
			primitive.baseIndex,
			primitive.baseVertex,
			primitive.baseInstance
		);
	}

	context.commandBuffer.endRendering();
}
