#include "VoxelizationPass.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "TaskContext.hpp"

static const uint16_t VOXELGRID_SIZE = 1024;

void VoxelizationParametersUploadPass(TaskContext& context);

void VoxelizationPass(TaskContext& context) {
	vk::RenderingInfo renderingInfo {
		.layerCount = 0,
		.viewMask = 0,

	};
	renderingInfo.renderArea = vk::Rect2D(
		{ 0, 0 },
		{
			VOXELGRID_SIZE,
			VOXELGRID_SIZE,
		}
	),
	context.commandBuffer.beginRendering(renderingInfo);
	context.commandBuffer.setScissor(
		0,
		{
			vk::Rect2D { { 0, 0 }, { VOXELGRID_SIZE, VOXELGRID_SIZE } }
    }
	);

	context.commandBuffer.setViewport(
		0,
		{
			vk::Viewport {
						  0, 0, (float)VOXELGRID_SIZE, (float)VOXELGRID_SIZE, 0, 1 }
    }
	);

	Buffer& positionBuffer =
		context.resourceManager.getNamedBuffer("vertex_positions_buffer");
	Buffer& attributeBuffer =
		context.resourceManager.getNamedBuffer("vertex_attributes_buffer");
	Buffer& instanceBuffer =
		context.resourceManager.getNamedBuffer("instance_buffer");
	Buffer& indexBuffer =
		context.resourceManager.getNamedBuffer("index_buffer");

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
	MaterialIndex materialIndex =
		context.materialManager.getMaterialIndex("voxelization");

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

	TaskContext::Descriptors descriptors = context.getDescriptors(true);

	context.commandBuffer.pushDescriptorSet(
		vk::PipelineBindPoint::eGraphics,
		material.pipelineLayout,
		0,
		descriptors.descriptors
	);

	auto& primitives = context.scene.buckets.at(
		context.materialManager.getMaterialIndex("gbuffer")
	);

	float sceneSize = context.scene.size;

	std::array<glm::mat4, 2> viewProj;
	viewProj[0] = glm::lookAt(
		glm::vec3(0, -sceneSize, 0), glm::vec3(0), glm::vec3(0, 0, 1)
	);

	viewProj[1] = glm::orthoZO(
		-sceneSize, sceneSize, -sceneSize, sceneSize, 0.0f, sceneSize * 2
	);

	context.commandBuffer.pushConstants(
		material.pipelineLayout,
		vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry,
		0,
		sizeof(viewProj),
		&viewProj
	);

	for (const uint32_t primitiveIndex : primitives) {
		const Primitive& primitive = context.scene.primitives[primitiveIndex];

		context.commandBuffer.drawIndexed(
			primitive.indexCount,
			1,
			primitive.baseIndex,
			primitive.baseVertex,
			0
		);
	}

	context.commandBuffer.endRendering();
}
